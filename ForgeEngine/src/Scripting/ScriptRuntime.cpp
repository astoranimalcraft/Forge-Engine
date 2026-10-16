#include "ScriptRuntime.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <system_error>

namespace
{
    constexpr char Base64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string Base64Encode(const std::string& input)
    {
        std::string out; out.reserve(((input.size()+2)/3)*4);
        unsigned int value=0; int bits=-6;
        for (unsigned char c : input) { value=(value<<8)|c; bits+=8; while(bits>=0){out.push_back(Base64Alphabet[(value>>bits)&0x3F]);bits-=6;} }
        if(bits>-6) out.push_back(Base64Alphabet[((value<<8)>>(bits+8))&0x3F]);
        while(out.size()%4) out.push_back('=');
        return out;
    }
    std::string Base64Decode(const std::string& input)
    {
        std::array<int,256> table{}; table.fill(-1);
        for(int i=0;i<64;++i)table[static_cast<unsigned char>(Base64Alphabet[i])]=i;
        std::string out; unsigned int value=0; int bits=-8;
        for(unsigned char c:input){if(c=='=')break;if(table[c]<0)continue;value=(value<<6)|static_cast<unsigned int>(table[c]);bits+=6;if(bits>=0){out.push_back(static_cast<char>((value>>bits)&0xFF));bits-=8;}}
        return out;
    }
    std::vector<std::string> Split(const std::string& line, char delimiter='|')
    {
        std::vector<std::string> values; size_t start=0;
        for(;;){size_t at=line.find(delimiter,start);if(at==std::string::npos){values.emplace_back(line.substr(start));break;}values.emplace_back(line.substr(start,at-start));start=at+1;}
        return values;
    }
    int ToInt(const std::string& value,int fallback=0){char* end=nullptr;long n=std::strtol(value.c_str(),&end,10);return end!=value.c_str()?static_cast<int>(n):fallback;}
    float ToFloat(const std::string& value,float fallback=0.0f){char* end=nullptr;float n=std::strtof(value.c_str(),&end);return end!=value.c_str()&&std::isfinite(n)?n:fallback;}
    std::wstring Quote(const std::filesystem::path& path)
    {
        const std::wstring value=path.wstring();std::wstring result=L"\"";
        for(wchar_t ch:value){if(ch==L'\"')result+=L"\\\"";else result+=ch;}result+=L"\"";return result;
    }
    std::filesystem::path FindDotnet()
    {
        std::array<wchar_t,32768> buffer{};
        DWORD n=SearchPathW(nullptr,L"dotnet.exe",nullptr,static_cast<DWORD>(buffer.size()),buffer.data(),nullptr);
        if(n>0&&n<buffer.size())return std::filesystem::path(buffer.data());
        const wchar_t* envNames[] = { L"DOTNET_ROOT", L"DOTNET_ROOT_X64", L"DOTNET_ROOT(x86)", L"ProgramFiles", L"ProgramFiles(x86)", L"LocalAppData" };
        for (const wchar_t* envName : envNames)
        {
            std::array<wchar_t,32768> env{};
            n=GetEnvironmentVariableW(envName,env.data(),static_cast<DWORD>(env.size()));
            if(n==0||n>=env.size()) continue;
            std::filesystem::path base(env.data());
            std::vector<std::filesystem::path> candidates;
            if (std::wstring(envName).find(L"ProgramFiles") == 0)
                candidates.push_back(base/L"dotnet"/L"dotnet.exe");
            else if (std::wstring(envName).find(L"LocalAppData") == 0)
                candidates.push_back(base/L"Microsoft"/L"dotnet"/L"dotnet.exe");
            else candidates.push_back(base/L"dotnet.exe");
            for (const auto& candidate : candidates) { std::error_code ec; if(std::filesystem::exists(candidate,ec)) return candidate; }
        }
        return {};
    }

    bool RetargetApiReference(const std::filesystem::path& sourceRoot,
        const std::filesystem::path& scriptsProject, std::vector<std::string>& logs)
    {
        const auto apiProject = sourceRoot/L"Scripting"/L"ForgeEngine.API"/L"ForgeEngine.API.csproj";
        std::error_code ec;
        if (!std::filesystem::exists(apiProject, ec))
        {
            logs.push_back("[C# build] ERROR: ForgeEngine.API.csproj not found at " + apiProject.string());
            return false;
        }
        std::ifstream in(scriptsProject, std::ios::binary);
        if (!in) { logs.push_back("[C# build] ERROR: Could not read " + scriptsProject.string()); return false; }
        std::ostringstream buffer; buffer << in.rdbuf();
        std::string xml = buffer.str();
        const auto tag = xml.find("<ProjectReference");
        if (tag == std::string::npos) { logs.push_back("[C# build] ERROR: Scripts project has no ProjectReference to ForgeEngine.API."); return false; }
        const auto include = xml.find("Include=\"", tag);
        if (include == std::string::npos) { logs.push_back("[C# build] ERROR: Malformed ProjectReference Include attribute."); return false; }
        const auto valueBegin = include + 9;
        const auto valueEnd = xml.find('"', valueBegin);
        if (valueEnd == std::string::npos) { logs.push_back("[C# build] ERROR: Unterminated ProjectReference Include attribute."); return false; }
        auto relative = std::filesystem::relative(apiProject, scriptsProject.parent_path(), ec);
        std::string desired = ec ? apiProject.generic_string() : relative.generic_string();
        std::replace(desired.begin(), desired.end(), '\\', '/');
        if (xml.substr(valueBegin, valueEnd-valueBegin) != desired)
        {
            xml.replace(valueBegin, valueEnd-valueBegin, desired);
            std::ofstream out(scriptsProject, std::ios::binary|std::ios::trunc);
            if (!out) { logs.push_back("[C# build] ERROR: Could not update API reference in scripts .csproj."); return false; }
            out << xml;
        }
        return true;
    }
    void ReadBuildLog(const std::filesystem::path& path,std::vector<std::string>& logs)
    {
        std::ifstream in(path,std::ios::binary);std::string line;
        while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(!line.empty())logs.push_back("[C# build] "+line);}
    }
}

ForgeScriptRuntime::~ForgeScriptRuntime(){Stop();}

bool ForgeScriptRuntime::RunDotnetBuild(const std::filesystem::path& dotnetPath,const std::filesystem::path& projectFile,
    const std::filesystem::path& workingDirectory,const std::filesystem::path& logFile,std::vector<std::string>& logs)
{
    std::error_code ec;std::filesystem::create_directories(logFile.parent_path(),ec);
    SECURITY_ATTRIBUTES inheritHandles{}; inheritHandles.nLength = sizeof(inheritHandles); inheritHandles.bInheritHandle = TRUE;
    HANDLE logHandle=CreateFileW(logFile.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&inheritHandles,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(logHandle==INVALID_HANDLE_VALUE){logs.push_back("[C# build] ERROR: Could not create build log: "+logFile.string());return false;}
    HANDLE nullInput=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&inheritHandles,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    startup.hStdInput=nullInput!=INVALID_HANDLE_VALUE?nullInput:GetStdHandle(STD_INPUT_HANDLE);startup.hStdOutput=logHandle;startup.hStdError=logHandle;
    std::wstring args=Quote(dotnetPath)+L" build "+Quote(projectFile)+L" --configuration Debug --nologo --verbosity minimal -p:RestoreIgnoreFailedSources=true";
    std::vector<wchar_t> mutableArgs(args.begin(),args.end());mutableArgs.push_back(L'\0');PROCESS_INFORMATION process{};
    BOOL created=CreateProcessW(dotnetPath.c_str(),mutableArgs.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,workingDirectory.c_str(),&startup,&process);
    if(nullInput!=INVALID_HANDLE_VALUE)CloseHandle(nullInput);CloseHandle(logHandle);
    if(!created){logs.push_back("[C# build] ERROR: Could not launch dotnet.exe at '" + std::string(dotnetPath.string()) + "'. Install a .NET 8 SDK or add dotnet.exe to PATH.");return false;}
    DWORD wait=WaitForSingleObject(process.hProcess,120000);
    if(wait==WAIT_TIMEOUT){TerminateProcess(process.hProcess,2);WaitForSingleObject(process.hProcess,1000);logs.push_back("[C# build] ERROR: dotnet build timed out after 120 seconds.");}
    DWORD exitCode=1;GetExitCodeProcess(process.hProcess,&exitCode);CloseHandle(process.hThread);CloseHandle(process.hProcess);ReadBuildLog(logFile,logs);
    if(wait==WAIT_TIMEOUT)return false;
    if(exitCode!=0)logs.push_back("[C# build] ERROR: Build failed (exit code "+std::to_string(exitCode)+").");
    return exitCode==0;
}

bool ForgeScriptRuntime::Build(const std::filesystem::path& sourceRoot,const std::filesystem::path& projectRoot,std::vector<std::string>& logs)
{
    const auto dotnet=FindDotnet();if(dotnet.empty()){logs.push_back("ERROR: .NET SDK not found. Install the .NET 8 SDK and restart Forge Engine.");return false;}
    const auto scriptsProject=projectRoot/L"Scripts"/L"ForgeGameScripts.csproj";
    const auto hostProject=sourceRoot/L"Scripting"/L"ForgeScriptHost"/L"ForgeScriptHost.csproj";
    if(!std::filesystem::exists(scriptsProject)){logs.push_back("ERROR: C# scripts project missing: "+scriptsProject.string());return false;}
    if(!std::filesystem::exists(hostProject)){logs.push_back("ERROR: ForgeScriptHost project missing: "+hostProject.string());return false;}
    logs.push_back("Compiling ForgeGameScripts.dll and the ForgeEngine C# API...");
    if (!RetargetApiReference(sourceRoot, scriptsProject, logs)) return false;
    if(!RunDotnetBuild(dotnet,scriptsProject,scriptsProject.parent_path(),projectRoot/L"Scripts"/L"build.log",logs))return false;
    if(!RunDotnetBuild(dotnet,hostProject,hostProject.parent_path(),projectRoot/L"Scripts"/L"host-build.log",logs))return false;
    logs.push_back("C# compilation succeeded: Project/Scripts/bin/Debug/net8.0/ForgeGameScripts.dll");return true;
}

bool ForgeScriptRuntime::Start(const std::filesystem::path& sourceRoot,const std::filesystem::path& projectRoot,std::vector<std::string>& logs)
{
    Stop();const auto dotnet=FindDotnet();
    const auto hostDll=sourceRoot/L"Scripting"/L"ForgeScriptHost"/L"bin"/L"Debug"/L"net8.0"/L"ForgeScriptHost.dll";
    const auto gameDll=projectRoot/L"Scripts"/L"bin"/L"Debug"/L"net8.0"/L"ForgeGameScripts.dll";std::error_code ec;
    if(dotnet.empty()||!std::filesystem::exists(hostDll,ec)||!std::filesystem::exists(gameDll,ec)){logs.push_back("ERROR: ForgeScriptHost.dll or ForgeGameScripts.dll is missing. Compile scripts first.");return false;}
    SECURITY_ATTRIBUTES sa{};sa.nLength=sizeof(sa);sa.bInheritHandle=TRUE;HANDLE childIn=nullptr,parentIn=nullptr,parentOut=nullptr,childOut=nullptr;
    if(!CreatePipe(&childIn,&parentIn,&sa,0)||!CreatePipe(&parentOut,&childOut,&sa,0)){
        if(childIn)CloseHandle(childIn);if(parentIn)CloseHandle(parentIn);if(parentOut)CloseHandle(parentOut);if(childOut)CloseHandle(childOut);
        logs.push_back("ERROR: Could not create C# script host pipes.");return false;
    }
    SetHandleInformation(parentIn,HANDLE_FLAG_INHERIT,0);SetHandleInformation(parentOut,HANDLE_FLAG_INHERIT,0);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    startup.hStdInput=childIn;startup.hStdOutput=childOut;startup.hStdError=childOut;
    std::wstring command=Quote(dotnet)+L" "+Quote(hostDll)+L" "+Quote(gameDll);std::vector<wchar_t> mutableCommand(command.begin(),command.end());mutableCommand.push_back(L'\0');PROCESS_INFORMATION process{};
    const auto working=(projectRoot/L"Scripts").wstring();
    BOOL created=CreateProcessW(dotnet.c_str(),mutableCommand.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,working.c_str(),&startup,&process);
    CloseHandle(childIn);CloseHandle(childOut);
    if(!created){CloseHandle(parentIn);CloseHandle(parentOut);logs.push_back("ERROR: Could not launch C# script host. Check the .NET 8 runtime installation.");return false;}
    m_process=process;m_childInputWrite=parentIn;m_childOutputRead=parentOut;m_readBuffer.clear();
    const ULONGLONG deadline=GetTickCount64()+20000;std::string line;
    while(GetTickCount64()<deadline)
    {
        if(!ReadProtocolLine(line,250))
        {
            if(WaitForSingleObject(m_process.hProcess,0)==WAIT_OBJECT_0)
            {
                logs.push_back("ERROR: C# host exited before becoming ready. Check the .NET runtime and the Console diagnostics.");
                Stop(); return false;
            }
            continue;
        }
        AppendHostLog(line,logs);auto p=Split(line);
        if(p.size()>=2&&p[0]=="HOST"&&p[1]=="READY"){logs.push_back("C# script runtime is ready. Start/Update and Debug.Log are active in Play mode.");return true;}
        if(p.size()>=2&&p[0]=="HOST"&&p[1]=="ERROR"){Stop();return false;}
    }
    logs.push_back("ERROR: C# host did not become ready within 20 seconds.");Stop();return false;
}

void ForgeScriptRuntime::Stop(std::vector<std::string>* logs)
{
    if(m_process.hProcess)
    {
        if(m_childInputWrite)
        {
            const char stop[]="STOP\n";DWORD written=0;
            WriteFile(m_childInputWrite,stop,static_cast<DWORD>(sizeof(stop)-1),&written,nullptr);
        }
        if(WaitForSingleObject(m_process.hProcess,500)==WAIT_TIMEOUT)
        {
            TerminateProcess(m_process.hProcess,0);
            WaitForSingleObject(m_process.hProcess,1000);
        }
        // Drain shutdown callbacks (OnDisable/OnDestroy and Debug.Log) before closing the pipe.
        if(logs && m_childOutputRead)
        {
            for(int i=0;i<256;++i)
            {
                std::string line;
                if(!ReadProtocolLine(line,2))break;
                if(line!="END")AppendHostLog(line,*logs);
            }
        }
        if(m_process.hThread)CloseHandle(m_process.hThread);
        CloseHandle(m_process.hProcess);m_process={};
        if(logs)logs->push_back("C# script runtime stopped.");
    }
    CloseHandles();m_readBuffer.clear();
}
void ForgeScriptRuntime::CloseHandles(){if(m_childInputWrite){CloseHandle(m_childInputWrite);m_childInputWrite=nullptr;}if(m_childOutputRead){CloseHandle(m_childOutputRead);m_childOutputRead=nullptr;}}

bool ForgeScriptRuntime::SendText(const std::string& text)
{
    if(!m_childInputWrite)return false;size_t offset=0;
    while(offset<text.size()){DWORD written=0;DWORD count=static_cast<DWORD>((std::min)(text.size()-offset,static_cast<size_t>(0x7fffffff)));
        if(!WriteFile(m_childInputWrite,text.data()+offset,count,&written,nullptr)||written==0)return false;offset+=written;}
    return true;
}

bool ForgeScriptRuntime::ReadProtocolLine(std::string& line,DWORD timeoutMilliseconds)
{
    auto extract=[&](){size_t at=m_readBuffer.find('\n');if(at==std::string::npos)return false;line.assign(m_readBuffer,0,at);m_readBuffer.erase(0,at+1);if(!line.empty()&&line.back()=='\r')line.pop_back();return true;};
    if(extract())return true;if(!m_childOutputRead)return false;const ULONGLONG deadline=GetTickCount64()+timeoutMilliseconds;
    do{DWORD available=0;if(!PeekNamedPipe(m_childOutputRead,nullptr,0,nullptr,&available,nullptr))return false;
        if(available>0){std::array<char,4096> buffer{};DWORD read=0;DWORD amount=(std::min)(available,static_cast<DWORD>(buffer.size()));
            if(!ReadFile(m_childOutputRead,buffer.data(),amount,&read,nullptr)||read==0)return false;m_readBuffer.append(buffer.data(),read);if(extract())return true;}
        if(m_process.hProcess&&WaitForSingleObject(m_process.hProcess,0)==WAIT_OBJECT_0){DWORD remaining=0;if(!PeekNamedPipe(m_childOutputRead,nullptr,0,nullptr,&remaining,nullptr)||remaining==0)return extract();}
        Sleep(1);
    }while(GetTickCount64()<=deadline);return extract();
}

void ForgeScriptRuntime::AppendHostLog(const std::string& line,std::vector<std::string>& logs)
{
    const auto p=Split(line);if(p.empty())return;
    if(p[0]=="LOG"&&p.size()>=3)logs.push_back("[C# "+p[1]+"] "+Base64Decode(p[2]));
    else if(p[0]=="HOST"&&p.size()>=3&&p[1]=="READY")logs.push_back("C# host ready; script types found: "+p[2]);
    else if(p[0]!="END")logs.push_back("[C# host] "+line);
}

bool ForgeScriptRuntime::Tick(float deltaSeconds,const ForgeScriptInputState& input,std::vector<ForgeScriptObjectState>& objects,std::vector<std::string>& logs)
{
    if(!IsRunning())return false;std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9);
    out<<"FRAME|"<<(std::isfinite(deltaSeconds)?std::max(0.0f,deltaSeconds):0.0f)<<'|'<<input.mouseX<<'|'<<input.mouseY
       <<'|'<<input.mousePositionX<<'|'<<input.mousePositionY<<'|'<<input.mouseButtons<<'|';
    for(size_t i=0;i<input.keysDown.size();++i){if(i)out<<',';out<<input.keysDown[i];}out<<'\n';
    for(const auto& mapping:m_idMappings)out<<"NEWID|"<<mapping.first<<'|'<<mapping.second<<'\n';
    m_idMappings.clear();
    m_pendingCommands.clear();
    for(const auto& o:objects){out<<"OBJ|"<<o.id<<'|'<<Base64Encode(o.name)<<'|'<<o.position.x<<'|'<<o.position.y<<'|'<<o.position.z<<'|'
        <<o.rotationDegrees.x<<'|'<<o.rotationDegrees.y<<'|'<<o.rotationDegrees.z<<'|'<<o.scale.x<<'|'<<o.scale.y<<'|'<<o.scale.z<<'|'
        <<(o.hasMeshRenderer?1:0)<<'|'<<(o.meshRendererEnabled?1:0)<<'|'<<o.lightType<<'|'<<(o.lightEnabled?1:0)<<'|'<<o.lightColor.x<<'|'<<o.lightColor.y<<'|'
        <<o.lightColor.z<<'|'<<o.lightIntensity<<'|'<<o.lightRange<<'|'<<o.lightInnerAngle<<'|'<<o.lightOuterAngle<<'|'
        <<(o.hasCollider?1:0)<<'|'<<(o.hasAudioSource?1:0)<<'|'<<(o.hasCamera?1:0)<<'|'
        <<Base64Encode(o.tag)<<'|'<<o.layer<<'|'<<o.colliderType<<'|'<<o.colliderSize.x<<'|'<<o.colliderSize.y<<'|'<<o.colliderSize.z<<'|'
        <<(o.isTrigger?1:0)<<'|'<<(o.hasRigidbody?1:0)<<'|'<<o.mass<<'|'<<(o.useGravity?1:0)<<'|'<<(o.isKinematic?1:0)<<'|'
         <<o.baseColor.x<<'|'<<o.baseColor.y<<'|'<<o.baseColor.z<<'|'<<o.metallic<<'|'<<o.roughness<<'|'
         <<o.emissiveColor.x<<'|'<<o.emissiveColor.y<<'|'<<o.emissiveColor.z<<'|'<<o.emissiveIntensity<<'|'
         <<o.normalMapScale<<'|'<<(o.doubleSided?1:0)<<'|'
         <<o.cameraFieldOfView<<'|'<<o.cameraNearClip<<'|'<<o.cameraFarClip<<'|'<<(o.cameraOrthographic?1:0)<<'|'
         <<o.cameraOrthographicSize<<'|'<<o.cameraDepth<<'|'
         <<o.cameraBackgroundColor.x<<'|'<<o.cameraBackgroundColor.y<<'|'<<o.cameraBackgroundColor.z<<'|'<<o.cameraBackgroundColor.w<<'|'
         <<(o.cameraDrawSkybox?1:0)<<'|'<<(o.cameraDrawGizmos?1:0)<<'|'<<o.cameraCullingMask<<'\n';
        for(const auto& s:o.scripts)out<<"SCRIPT|"<<o.id<<'|'<<s.className<<'|'<<(s.enabled?1:0)<<'\n';}
    out<<"END\n";if(!SendText(out.str())){logs.push_back("ERROR: Lost connection to the C# script host.");Stop();return false;}
    bool gotEnd=false;const ULONGLONG deadline=GetTickCount64()+3000;std::map<int,ForgeScriptObjectState*> byId;for(auto& o:objects)byId[o.id]=&o;
    while(GetTickCount64()<deadline){std::string line;if(!ReadProtocolLine(line,50)){if(!IsRunning())break;continue;}if(line=="END"){gotEnd=true;break;}auto p=Split(line);
        if(p.size()>=4&&p[0]=="SCRIPTSET")
        {
            const int id=ToInt(p[1],-1);auto it=byId.find(id);if(it==byId.end())continue;
            auto& state=*it->second;
            for(auto& script:state.scripts)if(script.className==p[2]){script.enabled=ToInt(p[3],script.enabled?1:0)!=0;break;}
        }
        else if(p.size()>=3&&p[0]=="NAMESET")
        {
            const int id=ToInt(p[1],-1);auto it=byId.find(id);if(it!=byId.end())it->second->name=Base64Decode(p[2]);
        }
        else if(p.size()>=3&&p[0]=="CMD"&&p[1]=="DEST")
        {
            ForgeScriptCommand command;command.kind=1;command.targetId=ToInt(p[2],-1);
            if(command.targetId>=0)m_pendingCommands.push_back(command);
        }
        else if(p.size()>=14&&p[0]=="CMD"&&p[1]=="INST")
        {
            ForgeScriptCommand command;command.kind=0;
            command.tempId=ToInt(p[2],-1);command.sourceId=ToInt(p[3],-1);command.name=Base64Decode(p[4]);
            command.position={ToFloat(p[5],0),ToFloat(p[6],0),ToFloat(p[7],0)};
            command.rotationDegrees={ToFloat(p[8],0),ToFloat(p[9],0),ToFloat(p[10],0)};
            command.scale={ToFloat(p[11],1),ToFloat(p[12],1),ToFloat(p[13],1)};
            if(command.sourceId>=0&&command.tempId<0)m_pendingCommands.push_back(command);
        }
        else if(p.size()>=20&&p[0]=="SET"){int id=ToInt(p[1],-1);auto it=byId.find(id);if(it==byId.end())continue;auto& o=*it->second;
            o.position={ToFloat(p[2],o.position.x),ToFloat(p[3],o.position.y),ToFloat(p[4],o.position.z)};
            o.rotationDegrees={ToFloat(p[5],o.rotationDegrees.x),ToFloat(p[6],o.rotationDegrees.y),ToFloat(p[7],o.rotationDegrees.z)};
            o.scale={ToFloat(p[8],o.scale.x),ToFloat(p[9],o.scale.y),ToFloat(p[10],o.scale.z)};
            o.meshRendererEnabled=ToInt(p[11],o.meshRendererEnabled?1:0)!=0;o.lightEnabled=ToInt(p[12],o.lightEnabled?1:0)!=0;
            o.lightColor={ToFloat(p[13],o.lightColor.x),ToFloat(p[14],o.lightColor.y),ToFloat(p[15],o.lightColor.z)};
            o.lightIntensity=ToFloat(p[16],o.lightIntensity);o.lightRange=ToFloat(p[17],o.lightRange);o.lightInnerAngle=ToFloat(p[18],o.lightInnerAngle);o.lightOuterAngle=ToFloat(p[19],o.lightOuterAngle);
            if(p.size()>=22){o.tag=Base64Decode(p[20]);o.layer=ToInt(p[21],o.layer);}
            if(p.size()>=31){o.colliderType=ToInt(p[22],o.colliderType);o.colliderSize={ToFloat(p[23],o.colliderSize.x),ToFloat(p[24],o.colliderSize.y),ToFloat(p[25],o.colliderSize.z)};
                o.isTrigger=ToInt(p[26],o.isTrigger?1:0)!=0;o.hasRigidbody=ToInt(p[27],o.hasRigidbody?1:0)!=0;o.mass=ToFloat(p[28],o.mass);
                o.useGravity=ToInt(p[29],o.useGravity?1:0)!=0;o.isKinematic=ToInt(p[30],o.isKinematic?1:0)!=0;}
            if(p.size()>=42){
                o.baseColor={ToFloat(p[31],o.baseColor.x),ToFloat(p[32],o.baseColor.y),ToFloat(p[33],o.baseColor.z)};
                o.metallic=std::clamp(ToFloat(p[34],o.metallic),0.0f,1.0f);
                o.roughness=std::clamp(ToFloat(p[35],o.roughness),0.045f,1.0f);
                o.emissiveColor={ToFloat(p[36],o.emissiveColor.x),ToFloat(p[37],o.emissiveColor.y),ToFloat(p[38],o.emissiveColor.z)};
                o.emissiveIntensity=std::clamp(ToFloat(p[39],o.emissiveIntensity),0.0f,64.0f);
                o.normalMapScale=std::clamp(ToFloat(p[40],o.normalMapScale),0.0f,8.0f);
                o.doubleSided=ToInt(p[41],o.doubleSided?1:0)!=0;
            }
            if(p.size()>=55){
                // SET omits HasMeshRenderer, so every field sits 6 tokens earlier than in OBJ.
                o.cameraFieldOfView=ToFloat(p[42],o.cameraFieldOfView);
                o.cameraNearClip=ToFloat(p[43],o.cameraNearClip);
                o.cameraFarClip=ToFloat(p[44],o.cameraFarClip);
                o.cameraOrthographic=ToInt(p[45],o.cameraOrthographic?1:0)!=0;
                o.cameraOrthographicSize=ToFloat(p[46],o.cameraOrthographicSize);
                o.cameraDepth=ToInt(p[47],o.cameraDepth);
                o.cameraBackgroundColor={ToFloat(p[48],o.cameraBackgroundColor.x),ToFloat(p[49],o.cameraBackgroundColor.y),ToFloat(p[50],o.cameraBackgroundColor.z),ToFloat(p[51],o.cameraBackgroundColor.w)};
                o.cameraDrawSkybox=ToInt(p[52],o.cameraDrawSkybox?1:0)!=0;
                o.cameraDrawGizmos=ToInt(p[53],o.cameraDrawGizmos?1:0)!=0;
                o.cameraCullingMask=ToInt(p[54],o.cameraCullingMask);
            }
        }else AppendHostLog(line,logs);
    }
    if(!gotEnd){logs.push_back("ERROR: C# host did not answer in 3 seconds. A script may be stuck in an infinite loop; the isolated host is stopping.");Stop();return false;}return true;
}
