#include <filesystem>
#include <memory>
#include <atomic>
#include <chrono>
#include <ctime>
#include "utils.hpp"
#include "process.hpp"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "template.hpp"
#include "TextEditor.h"

#include "Async.hpp"
#include <SDL2/SDL.h>
#include "portable-file-dialogs.h"

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

#include "nlohmann/json.hpp"
using json = nlohmann::json;
namespace fs = std::filesystem;
int Window_Width = 1280;
int Window_Height = 920;
std::string consoleOutput;
std::string currentDir;

enum class BuildType
{
    STATIC = 0,
    SHARED
};

class Module
{
public:
    Module(const std::string &fileName) : filename(fileName)
    {
        path = fs::path(fileName).parent_path().string() + pathSeparator;
        std::cout << "Load Module: " << fileName << std::endl;


        linuxArgs = " -I" + path + "include  -I" + path + "src -I" + currentDir+pathSeparator + "modules" + pathSeparator + "include -I" + currentDir + "modules" + pathSeparator + "include" + pathSeparator + "Linux ";
        androidArgs = " -I" + path + "include  -I" + path + "src -I" + currentDir+pathSeparator + "modules" + pathSeparator + "include -I" + currentDir + "modules" + pathSeparator + "include" + pathSeparator + "Android ";
        emscriptenArgs = " -I" + path + "include  -I" + path + "src -I" + currentDir+pathSeparator + "modules" + pathSeparator + "include -I" + currentDir + "modules" + pathSeparator + "include" + pathSeparator + "Web ";

        linuxLdArgs= " -L" + currentDir +pathSeparator+ "modules" + pathSeparator + "libs" + pathSeparator + "Linux ";
        androidLdArgs= " -L" + currentDir +pathSeparator+ "modules" + pathSeparator + "libs" + pathSeparator + "Android ";
        emscriptenLdArgs= " -L" + currentDir +pathSeparator+ "modules" + pathSeparator + "libs" + pathSeparator + "Web ";


        Load();
    }
    ~Module()
    {
        //   std::cout << "Module destroyed" << std::endl;
    }

    bool CompileLinux(bool isStatic);

    bool CompileAndroid(bool isStatic);

    bool CompileEmscripten();
    bool BuildLinux(bool isStatic);

    bool BuildAndroid(bool isStatic);

    bool BuildEmscripten();

    bool Load()
    {
        try
        {

            std::ifstream file(filename);
            if (!file.is_open())
            {
                throw std::runtime_error("Failed to open JSON file.");
            }

            std::string jsonData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            // Fazer o parsing do JSON
            json parsedJson = json::parse(jsonData);

            // Acessar os dados do JSON
            moduleName = parsedJson["module"];
            about = parsedJson["about"];
            author = parsedJson["author"];
            version = parsedJson["version"];
            isCpp = parsedJson["CPP"];

            srcFiles = parsedJson["src"];
            includeDirs = parsedJson["include"];

            json plataforms = parsedJson["plataforms"];

            // // Acessar os dados da plataforma Linux
            json linuxPlatform = plataforms["linux"];
            //   std::cout << "linuxPlatform: " << linuxPlatform << std::endl;

            linuxArgs += linuxPlatform["ARGS"];
            linuxLdArgs += linuxPlatform["LD_ARGS"];
            linuxSrcFiles = linuxPlatform["src"];
            linuxIncludeDirs = linuxPlatform["include"];

            // // Acessar os dados da plataforma Android
            json androidPlatform = plataforms["android"];
            androidArgs += androidPlatform["ARGS"];
            androidLdArgs += androidPlatform["LD_ARGS"];
            androidSrcFiles = androidPlatform["src"];
            androidIncludeDirs = androidPlatform["include"];

            // // Acessar os dados da plataforma Emscripten
            json emscriptenPlatform = plataforms["emscripten"];
            emscriptenArgs += emscriptenPlatform["ARGS"];
            emscriptenLdArgs += emscriptenPlatform["LD_ARGS"];
            emscriptenSrcFiles = emscriptenPlatform["src"];
            emscriptenIncludeDirs = emscriptenPlatform["include"];


            std::string module_root = currentDir + pathSeparator + "modules" + pathSeparator  ;
          
            for (const auto inc: includeDirs )
            {
                if (inc.empty())
                    continue;
                linuxArgs += " -I" + module_root + inc + " ";
                androidArgs += " -I" + module_root + inc + " ";
                emscriptenArgs += " -I" + module_root + inc + " ";
            }
            
//-I/media/djoker/data/code/projectos/SDLBuilder/modules/src/raylib/external/glfw/include 

            for (const auto inc: linuxIncludeDirs )
            {
                if (inc.empty())
                    continue;
                linuxArgs += " -I" + module_root + inc + " ";

            }

            // linuxArgs += " -I" + path + "src ";
            // androidArgs += " -I" + path + "src ";
            // emscriptenArgs += " -I" + path + "src ";


            std::cout << "Module: " << moduleName << std::endl;
            std::cout << "Version: " << version << std::endl;
            std::cout << "Compile Args: " << linuxArgs << std::endl;
            std::cout << "Link Args: " << linuxLdArgs << std::endl;



            file.close();
            return true;
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Error: " << ex.what() << std::endl;
            return false;
        }
    }

    bool isCpp{false};
    std::string filename;
    std::string path;

    std::string moduleName;
    std::string about;
    std::string author;
    std::string version;

    std::vector<std::string> obsFiles;
    std::vector<std::string> srcFiles;
    std::vector<std::string> includeDirs;

    std::string linuxArgs;
    std::string linuxLdArgs;
    std::vector<std::string> linuxSrcFiles;
    std::vector<std::string> linuxIncludeDirs;

    std::string androidArgs;
    std::string androidLdArgs;
    std::vector<std::string> androidSrcFiles;
    std::vector<std::string> androidIncludeDirs;

    std::string emscriptenArgs;
    std::string emscriptenLdArgs;
    std::vector<std::string> emscriptenSrcFiles;
    std::vector<std::string> emscriptenIncludeDirs;
};

std::vector<std::string> getJsonFileNames(const std::string &folderPath)
{
    std::vector<std::string> fileNames;

    for (const auto &entry : fs::directory_iterator(folderPath))
    {
        const std::string &filePath = entry.path().string();
        if (filePath.substr(filePath.find_last_of(".") + 1) == "json")
        {
            fileNames.push_back(fs::path(filePath).filename().string());
        }
    }

    return fileNames;
}

std::vector<std::string> getJsonFileNamesRecursion(const std::string &folderPath)
{
    std::vector<std::string> fileNames;
    int step = 0;
    for (const auto &entry : fs::recursive_directory_iterator(folderPath))
    {
        const std::string &filePath = entry.path().string();
        const std::string &fileName = fs::path(filePath).filename().string();
        if (filePath.substr(filePath.find_last_of(".") + 1) == "json" && fileName == "module.json")
        {
            //        std::cout << filePath << std::endl;
            fileNames.push_back(fs::path(filePath).filename().string());
            continue;
        }
    }

    return fileNames;
}

void ChangeTheme(ImGuiStyle &style, ImGuiStyle &backupStyle, const std::string &theme)
{
    if (theme == "Dark")
    {
        ImGui::StyleColorsDark(&style);
    }
    else if (theme == "Light")
    {
        ImGui::StyleColorsLight(&style);
    }
    else if (theme == "XP")
    {
        ImGui::StyleColorsXP(&style);
    }
    else if (theme == "VSModernDark")
    {
        ImGui::StyleColorsVSModernDark(&style);
    }
    else
    {
        ImGui::StyleColorsClassic(&style);
    }

    backupStyle = style;
}

void setFontWithSize(const char *fontPath, float fontSize)
{
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.Fonts->AddFontFromFileTTF(fontPath, fontSize);
    io.FontDefault = io.Fonts->AddFontFromFileTTF(fontPath, fontSize);
}


static bool loadModules = false;
static bool abortCompilation = false;
static bool isCompiling = false;
static int processProgress = 0;
static int maxProgress = 0;
std::mutex progressMutex;

std::vector<bool> selectedItems;

using ModulePtr = std::shared_ptr<Module>;
std::vector<ModulePtr> moduleList;
std::vector<std::string> fileNames;

long int getFileWriteTime(const std::string &filePath)
{
    if (fs::exists(filePath))
    {
         struct stat fileStat;
        if (stat(filePath.c_str(), &fileStat) == 0) 
            return fileStat.st_mtime;
        return 0;
    }
    else
    {
      //  std::cout << "File not found: " << filePath << std::endl;
        return 0;
    }
}

// int64_t getFileWriteTime(const std::string &filePath)
// {
//     if (fs::exists(filePath))
//     {
//         auto lastModifiedTime = fs::last_write_time(filePath);
//         auto duration = lastModifiedTime.time_since_epoch();
//         //       return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
//         return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000;
//     }
//     else
//     {
//         std::cout << "File not found: " << filePath << std::endl;
//         return -1;
//     }
// }

void createDirectories(const std::string &path)
{
    try
    {
        fs::create_directories(path);
        std::cout << "Folder created: " << path << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error creating folder: " << ex.what() << std::endl;
    }
}

void removeFolder(const std::string &folderPath)
{
    try
    {
        fs::remove_all(folderPath);
        std::cout << "Folder removed: " << folderPath << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error removing folder: " << ex.what() << std::endl;
    }
}
void AddLog(const std::string &log)
{

    consoleOutput += log + "\n";
}

void ProcessLinuxCodeCompile(bool  isStatic,const std::string &compiler, const std::vector<std::string> &code, const std::vector<std::string> &objs, const std::string &args, int id, int startIdx, int endIdx)
{
    std::cout << "Thread id: " << id << " starts: " << startIdx << " end: " << endIdx << std::endl;

    std::string finalArgs = args;
    
    if ( !isStatic)
         finalArgs +=" -shared -fPIC ";



    for (int i = startIdx; i < endIdx; ++i)
    {
        if (abortCompilation)
            return;
       

        int64_t srtTime   = getFileWriteTime(code[i]);
        int64_t buildTime = getFileWriteTime(objs[i]);
        std::string command = compiler + " " + finalArgs + " -c " + code[i] + " -o " + objs[i];
        std::string output = "";

        if (srtTime >= buildTime)
        {
            std::lock_guard<std::mutex> lock(progressMutex);
            processProgress++;
            float progressPercentage = (static_cast<float>(processProgress) / maxProgress) * 100;
            AddLog("Compile [" + std::to_string((int)progressPercentage) + "]% " + code[i] + " > " + objs[i]);
            output = run_command(command);
        }
        else
        {
            AddLog("Skipping: " + code[i]);
            std::lock_guard<std::mutex> lock(progressMutex);
            processProgress++;
            continue;
        }


        if (output != "")
        {
            replace_character(output, "‘", "'");
            replace_character(output, "’", "'");
            std::vector<std::string> message= get_result(output);
            if (message.size() >= 5)
            {
                 if (message[4]=="warning")
                {
                    AddLog("Warning: " + message[0] +  "\n Message:   (" + message[5]+")" );
                    
                }else
                if (message[4]=="error")
                {
                    AddLog("Error: " + message[0] +  "\n  Message:   (" + message[5]+")" );
                    abortCompilation = true;
                     AddLog("Aborting compilation");
                     return;
                }else
                if (message[4]=="fatal error")
                {
                    AddLog("Fatal Error: " + message[0] + " (" + message[5]+")" );
                    abortCompilation = true;
                     AddLog("Aborting compilation");
                     return;
                }
            } else
            {
                AddLog(output);
            }
        }
    }
}

bool Module::CompileLinux(bool isStatic)
{
    std::vector<std::string> code;
    obsFiles.clear();


    std::string outPath = path + "obj/Linux/";

    if (!fs::exists(outPath) && !fs::is_directory(outPath))
    {
        createDirectories(outPath);
    }

    for (const auto &srcFile : srcFiles)
    {
        if (srcFile.empty())
            continue;
        std::string fileName =path+"src/"+ srcFile;
        if (!fs::exists(fileName))
        {
            AddLog  ("File not found: " + fileName);
            continue;
        }
    

        code.push_back(fileName);
        std::string objFile = srcFile.substr(0, srcFile.find_last_of(".")) + ".o";
        obsFiles.push_back(outPath + objFile);

        std::string sub_path = fs::path(outPath + objFile).parent_path().string() + pathSeparator;

        if (!fs::exists(sub_path) && !fs::is_directory(sub_path))
        {
            createDirectories(sub_path);
        }
    }
    for (const auto &srcFile : linuxSrcFiles)
    {
        if (srcFile.empty())
            continue;
        std::string fileName =path+"src/"+ srcFile;

        if (!fs::exists(fileName))
        {
            AddLog  ("File not found: " + fileName);
            continue;
        }
        code.push_back(fileName);
        std::string objFile = srcFile.substr(0, srcFile.find_last_of(".")) + ".o";
        obsFiles.push_back(outPath + objFile);

        std::string sub_path = fs::path(outPath + objFile).parent_path().string() + pathSeparator;

        if (!fs::exists(sub_path) && !fs::is_directory(sub_path))
        {
            createDirectories(sub_path);
        }
    }

    int totalCode = code.size();
    int numThreads = 1;
    if (totalCode >= 21 && totalCode <= 40)
    {
        numThreads = 2;
    }
    else if (totalCode >= 41 && totalCode <= 100)
    {
        numThreads = 3;
    }
    else if (totalCode >= 101 && totalCode <= 200)
    {
        numThreads = 4;
    }
    else if (totalCode >= 201)
    {
        numThreads = 5;
    }

    int blockSize = totalCode / numThreads;

    std::vector<std::thread> threads;

    std::string compiler;
    if (isCpp)
    {
        compiler = "g++";
    }
    else
    {
        compiler = "gcc";
    }
    processProgress = 0;
    maxProgress = totalCode;
    for (int i = 0; i < numThreads; ++i)
    {
        int startIdx = i * blockSize;
        int endIdx = (i == numThreads - 1) ? totalCode : startIdx + blockSize;
        threads.emplace_back(ProcessLinuxCodeCompile, isStatic,compiler, code, obsFiles, linuxArgs, i, startIdx, endIdx);
    }
    isCompiling = true;
    for (auto &thread : threads)
    {
        thread.join();
    }
    isCompiling = false;
    if (abortCompilation)
    {
        abortCompilation = false;
        return false;
    }
    AddLog("Compile: " + moduleName + " finished ;) ");

    return true;
}
void ProcessLinuxBuild(bool isStatic, const std::string &moduleName, const std::string &compiler, const std::vector<std::string> &objs, const std::string &args)
{
    AddLog("Build:  Linux " + compiler + " " + moduleName);
    std::string buildType;
    std::string extraArgs;

    std::string allObjs = "";
    for (const auto &obj : objs)
    {
        allObjs += obj + " ";
    }
    // std::string command = compiler + " " + allObjs + " -o " + currentDir + "/libs/Linux/lib" + moduleName + buildType+" "+ extraArgs+" " + args ;
    std::string command ="";
    std::string outPath =""; 
    
    if (isStatic)
    {
        outPath = currentDir + "/modules/libs/Linux/libs"+moduleName+".a"; 
        command =  "ar -r -s  "+outPath +" " +allObjs;
    }
    else
    {
     outPath = currentDir + "/modules/libs/Linux/lib"+moduleName+".so"; 
     command = compiler +" -shared -fPIC -o "+outPath+" "+allObjs+" "+args;
    }
    
    std::cout << "Build:  Linux " << command << std::endl;
    AddLog("Save to " + outPath);
    std::string output = run_command(command);
    if (!output.empty())
    {
        AddLog("Build: " + output);
    } else
    {
        AddLog("Build: " + moduleName + " finished ;) ");
    }
  //  std::cout << "Build: " << output << std::endl;
}
bool Module::BuildLinux(bool isStatic)
{
    std::string compiler;
    if (isCpp)
    {
        compiler = "g++";
    }
    else
    {
        compiler = "gcc";
    }
    std::thread thread(ProcessLinuxBuild, isStatic, moduleName, compiler, obsFiles, linuxLdArgs);
    thread.join();
    return true;
}


void ProcessEmscriptenCodeCompile(const std::string &compiler, const std::vector<std::string> &code, const std::vector<std::string> &objs, const std::string &args, int id, int startIdx, int endIdx)
{
    std::cout << "Thread id: " << id << " starts: " << startIdx << " end: " << endIdx << std::endl;
    std::string finalArgs =args;

    for (int i = startIdx; i < endIdx; ++i)
    {
        if (abortCompilation)
            return;
       
        int64_t srtTime   = getFileWriteTime(code[i]);
        int64_t buildTime = getFileWriteTime(objs[i]);
        std::string command = compiler + " " + finalArgs + " -c " + code[i] + " -o " + objs[i];
        std::string output = "";

        if (srtTime >= buildTime)
        {
            std::lock_guard<std::mutex> lock(progressMutex);
            processProgress++;
            float progressPercentage = (static_cast<float>(processProgress) / maxProgress) * 100;
            AddLog("Compile [" + std::to_string((int)progressPercentage) + "]% " + code[i] + " > " + objs[i]);
            output = run_command(command);
        }
        else
        {
            AddLog("Skipping: " + code[i]);
            std::lock_guard<std::mutex> lock(progressMutex);
            processProgress++;
            continue;
        }
 

        if (output != "")
        {
            replace_character(output, "‘", "'");
            replace_character(output, "’", "'");
            std::vector<std::string> message= get_result(output);
            if (message.size() >= 5)
            {
                 if (message[4]=="warning")
                {
                    AddLog("Warning: " + message[0] +  "\n Message:   (" + message[5]+")" );
                    
                }else
                if (message[4]=="error")
                {
                    AddLog("Error: " + message[0] +  "\n  Message:   (" + message[5]+")" );
                    abortCompilation = true;
                     AddLog("Aborting compilation");
                     return;
                }else
                if (message[4]=="fatal error")
                {
                    AddLog("Fatal Error: " + message[0] + " (" + message[5]+")" );
                    abortCompilation = true;
                    AddLog("Aborting compilation");
                    return;
                }
            } else
            {
                AddLog(output);
            }


          
        }
   

    


      
        //  std::cout << "Processing code thered: "<<id <<" src: "<< code[i] << std::endl;
    }
}
bool Module::CompileEmscripten()
{
    std::vector<std::string> code;
    obsFiles.clear();
    std::string outPath = path + "obj/Web/";

    if (!fs::exists(outPath) && !fs::is_directory(outPath))
    {
        createDirectories(outPath);
    }

    for (const auto &srcFile : srcFiles)
    {
        if (srcFile.empty())
            continue;
        std::string fileName =path+"src/"+ srcFile;
        if (!fs::exists(fileName))
        {
            AddLog  ("File not found: " + fileName);
            continue;
        }
    

        code.push_back(fileName);
        std::string objFile = srcFile.substr(0, srcFile.find_last_of(".")) + ".o";
        obsFiles.push_back(outPath + objFile);

        std::string sub_path = fs::path(outPath + objFile).parent_path().string() + pathSeparator;

        if (!fs::exists(sub_path) && !fs::is_directory(sub_path))
        {
            createDirectories(sub_path);
        }
    }
    for (const auto &srcFile : emscriptenSrcFiles)
    {
        if (srcFile.empty())
            continue;
        std::string fileName =path+"src/"+ srcFile;

        if (!fs::exists(fileName))
        {
            AddLog  ("File not found: " + fileName);
            continue;
        }
        code.push_back(fileName);
        std::string objFile = srcFile.substr(0, srcFile.find_last_of(".")) + ".o";
        obsFiles.push_back(outPath + objFile);

        std::string sub_path = fs::path(outPath + objFile).parent_path().string() + pathSeparator;

        if (!fs::exists(sub_path) && !fs::is_directory(sub_path))
        {
            createDirectories(sub_path);
        }
    }

    int totalCode = code.size();
    int numThreads = 1;
    if (totalCode >= 21 && totalCode <= 40)
    {
        numThreads = 2;
    }
    else if (totalCode >= 41 && totalCode <= 100)
    {
        numThreads = 3;
    }
    else if (totalCode >= 101 && totalCode <= 200)
    {
        numThreads = 4;
    }
    else if (totalCode >= 201)
    {
        numThreads = 5;
    }

    int blockSize = totalCode / numThreads;

    std::vector<std::thread> threads;

    std::string compiler;
    if (isCpp)
    {
        compiler = "em++";
    }
    else
    {
        compiler = "emcc";
    }
    processProgress = 0;
    maxProgress = totalCode;
    for (int i = 0; i < numThreads; ++i)
    {
        int startIdx = i * blockSize;
        int endIdx = (i == numThreads - 1) ? totalCode : startIdx + blockSize;
        threads.emplace_back(ProcessEmscriptenCodeCompile, compiler, code, obsFiles, emscriptenArgs, i, startIdx, endIdx);
    }
    isCompiling = true;
    for (auto &thread : threads)
    {
        thread.join();
    }
    isCompiling = false;
    if (abortCompilation)
    {
        abortCompilation = false;
        return false;
    }
    AddLog("Compile: " + moduleName + " finished ;) ");

    return true;
}

void ProcessBuildEmscripten(const std::string &moduleName, const std::string &compiler, const std::vector<std::string> &objs, const std::string &args)
{
    AddLog("Build:  Emscripten " + compiler + " " + moduleName);
    std::string buildType;
    std::string extraArgs;

    std::string allObjs = "";
    for (const auto &obj : objs)
    {
        allObjs += obj + " ";
    }
    // std::string command = compiler + " " + allObjs + " -o " + currentDir + "/libs/Linux/lib" + moduleName + buildType+" "+ extraArgs+" " + args ;
    std::string command ="";
    std::string outPath =""; 
    

    outPath = currentDir + "/modules/libs/Web/libs"+moduleName+".a"; 
    command =  "emar -r -s  "+outPath +" " +allObjs;

    
    std::cout << "Build:  Linux " << command << std::endl;
    AddLog("Save to " + outPath);
    std::string output = run_command(command);
    if (!output.empty())
    {
        AddLog("Build: " + output);
    } else
    {
        AddLog("Build: " + moduleName + " finished ;) ");
    }
  //  std::cout << "Build: " << output << std::endl;
}

bool Module::BuildEmscripten()
{
     std::string compiler;
    if (isCpp)
    {
        compiler = "em++";
    }
    else
    {
        compiler = "em++";
    }
    std::thread thread(ProcessBuildEmscripten, moduleName, compiler, obsFiles, emscriptenLdArgs);
    thread.join();
    return true;
}

bool Module::BuildAndroid(bool isStatic)
{
    return true;
}
bool Module::CompileAndroid(bool isStatic)
{
    return true;
}

void CompileModule(Module *module, bool isStatic, Platform plataform)
{

    AddLog("Compile: " + module->moduleName);
    bool result = false;
    if (module)
    {
        switch (plataform)
        {
        case Platform::Linux:
        {
            result = module->CompileLinux(isStatic);
        }
        break;
        case Platform::Android:
        {
            result = module->CompileAndroid(isStatic);
        }
        break;
        case Platform::Web:
        {
            result = module->CompileEmscripten();
        }
        break;
        }
    }
}

void BuildModule(Module *module, bool isStatic, Platform plataform)
{

    AddLog("Build: " + module->moduleName);
    bool result = false;
    if (module)
    {
        switch (plataform)
        {
        case Platform::Linux:
        {
            result = module->BuildLinux(isStatic);
        }
        break;
        case Platform::Android:
        {
            result = module->BuildAndroid(isStatic);
        }
        break;
        case Platform::Web:
        {
            result = module->BuildEmscripten();
        }
        break;
        }
    }
}

void LoadModules(const std::string &folderPath)
{
    loadModules = false;
    for (const auto &entry : fs::recursive_directory_iterator(folderPath))
    {
        const std::string &filePath = entry.path().string();
        const std::string &fileName = fs::path(filePath).filename().string();
        if (filePath.substr(filePath.find_last_of(".") + 1) == "json" && fileName == "module.json")
        {
            auto module = std::make_shared<Module>(filePath);
            selectedItems.push_back(false);
            moduleList.push_back(module);
            AddLog("Load Module: " + module.get()->moduleName);
            fileNames.push_back(module.get()->moduleName);
            std::cout << filePath << std::endl;
            continue;
        }

        // std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    loadModules = true;
}

int main(int, char **)
{

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("Cross Builder by DjokerSoft v.0.01", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, Window_Width, Window_Height, window_flags);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
    {
        SDL_Log("Error creating SDL_Renderer!");
        return false;
    }

    fs::path currentPath = fs::current_path();
    currentDir = currentPath.string();
    std::string folderPath = currentPath.string() + pathSeparator + "modules" + pathSeparator + "src";

    createDirectories("./modules/libs/Android");
    createDirectories("./modules/libs/Linux");
    createDirectories("./modules/libs/Web");
    createDirectories("./modules/include");
    createDirectories("./modules/include/Andoid");
    createDirectories("./modules/include/Web");
    createDirectories("./modules/include/Linux");

    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer, &info);
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    // ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    ImGui_ImplSDL2_InitForSDLRenderer(window);
    ImGui_ImplSDLRenderer_Init(renderer);

    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    int display_w, display_h;
    SDL_GetWindowSize(window, &display_w, &display_h);

    bool build_static = true;
    bool build_shared = false;
    bool build_android = false;
    bool build_windows = false;
    bool build_linux = true;
    bool build_web = false;
    bool build_arm7 = true;
    bool build_arm64 = false;
    bool build_x86 = false;

    ImGuiStyle &style = ImGui::GetStyle();

    ImGuiStyle backupStyle = style;
    ChangeTheme(style, backupStyle, "XP");
    std::string selectedTheme = "XP";
    style.ScaleAllSizes(2.0);

    std::thread loadThread(LoadModules, folderPath);
    loadThread.detach();

    float buttonWidth = 90.0f;
    float buttonHeight = 30.0f;
    std::string current_module = "";
    Platform select_platform = Platform::Linux;
      int screenshotIndex = 1;

    bool done = false;
    while (!done)
    {

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {

            switch (event.type)
            {
            case SDL_QUIT:
            {
                done = true;
                break;
            }
            case SDL_WINDOWEVENT:
            {
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                    done = true;
                else if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    SDL_GetWindowSize(window, &display_w, &display_h);
                    Window_Width = display_w;
                    Window_Height = display_h;
                }
                break;
            }
            case SDL_KEYDOWN:
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    done = true;

                break;
            }
            case SDL_KEYUP:
            {

                if (event.key.keysym.sym == SDLK_F12)
                {
                        std::stringstream ss;
                        ss << "screenshot" << screenshotIndex << ".bmp";
                        std::string screenshotFilename = ss.str();

                        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, Window_Width, Window_Height, 24, SDL_PIXELFORMAT_BGR888);
                        SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_BGR888, surface->pixels, surface->pitch);
                        SDL_SaveBMP(surface, screenshotFilename.c_str());
                        screenshotIndex++;
                        SDL_FreeSurface(surface);
                }

                break;
            }
            }
            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        const Uint8 *keyboardState = SDL_GetKeyboardState(nullptr);

        ImGui_ImplSDLRenderer_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const int maxItemsPerRow = 6;

        ImGui::Begin("Modules");

        if (ImGui::Button("Reload", ImVec2(buttonWidth, buttonHeight)))
        {
            
        }
        ImGui::Separator();
        if (loadModules)
        {
            // Configurar as colunas
            const float columnWidth = ImGui::GetContentRegionAvail().x / maxItemsPerRow;
            const int numColumns = (fileNames.size() + maxItemsPerRow - 1) / maxItemsPerRow;

            int index = 0;
            int current = 0;
            for (int i = 0; i < numColumns; i++)
            {
                ImGui::Columns(maxItemsPerRow, nullptr, false);
                const int startIndex = i * maxItemsPerRow;
                const int endIndex = std::min(startIndex + maxItemsPerRow, static_cast<int>(fileNames.size()));

                for (int j = startIndex; j < endIndex; j++)
                {
                    std::string label = fileNames[j] + "##" + std::to_string(index);
                    if (ImGui::RadioButton(label.c_str(), selectedItems[j]))
                    {
                        selectedItems[j] = true;
                        if (current_module != fileNames[j])
                        {
                            current_module = fileNames[j];
                            AddLog("Selected Module: " + current_module);
                        }
                        for (int k = 0; k < selectedItems.size(); ++k)
                        {
                            if (k != j)
                            {
                                selectedItems[k] = false;
                            }
                        }
                    }
                    index++;
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);
            }
        }
        ImGui::End();
        //********************
        ImGui::Begin("Build Options");

        if (!build_web)
        {
            if (ImGui::RadioButton("Static", build_static))
            {
                build_shared = false;
                build_static = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Shared", build_shared))
            {
                build_static = false;
                build_shared = true;
            }
        }
        if (build_android)
        {
               ImGui::Separator();
            if (ImGui::Checkbox("Arm", &build_arm7))
            {
                
            }
             ImGui::SameLine();
            if (ImGui::Checkbox("Arm64", &build_arm64))
            {
           

            }
            ImGui::SameLine();
            if (ImGui::Checkbox("x86", &build_x86))
            {
             
               

            }

        }
        ImGui::Separator();
        if (ImGui::RadioButton("Linux", build_linux))
        {
            build_android = false;
            build_windows = false;
            build_web = false;
            build_linux = true;
            select_platform = Platform::Linux;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Android", build_android))
        {
            build_linux = false;
            build_windows = false;
            build_web = false;
            build_android = true;
            select_platform = Platform::Android;
        }


        ImGui::SameLine();

        if (ImGui::RadioButton("Web", build_web))
        {
            build_linux = false;
            build_android = false;
            build_windows = false;
            build_web = true;
            select_platform = Platform::Web;
        }

        ImGui::Separator();
        if (isCompiling)
        {
            if (ImGui::Button("Abort", ImVec2(buttonWidth, buttonHeight)))
            {
                isCompiling = false;
                abortCompilation = true;
            }
               float progressPercentage = (static_cast<float>(processProgress) / maxProgress) ;

             ImGui::ProgressBar(progressPercentage, ImVec2(-1, 0), "Progresso");

        }
        else
        {

            if (ImGui::Button("Compile", ImVec2(buttonWidth, buttonHeight)))
            {
                consoleOutput = "";
                if (current_module != "")
                {

                    for (const auto &module : moduleList)
                    {
                        if (module.get()->moduleName == current_module)
                        {

                            std::thread loadThread(CompileModule, module.get(), build_static, select_platform);
                            loadThread.detach();
                            break;
                        }
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clean", ImVec2(buttonWidth, buttonHeight)))
            {
                consoleOutput = "";
                if (current_module != "")
                {

                    for (const auto &module : moduleList)
                    {

                        if (module.get()->moduleName == current_module)
                        {
                            std::string name = module.get()->moduleName;
                            std::string path = module.get()->path;
                            consoleOutput = "";
                            AddLog("Cleaning " + name);
                            switch (select_platform)
                            {
                            
                                case Platform::Linux:
                                {
                                 removeFolder(path + "obj/Linux");   
                                }
                                break;
                                case Platform::Android:
                                {
                                
                                 removeFolder(path + "obj/Android");   
                                }
                                break;
                                case Platform::Web:
                                {
                                 removeFolder(path + "obj/Web");   
                                }
                                break;
                            }

                            break;
                        }
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Build", ImVec2(buttonWidth, buttonHeight)))
            {
                consoleOutput = "";
                if (current_module != "")
                {

                    for (const auto &module : moduleList)
                    {
                        if (module.get()->moduleName == current_module)
                        {

                            std::thread loadThread(BuildModule, module.get(), build_static, select_platform);
                            loadThread.detach();
                            break;
                        }
                    }
                }
            }
        } // end else
        ImGui::End();
        //********************************************************************************
        ImGui::Begin("Console");
        if (ImGui::Button("Clean"))
        {
            consoleOutput = "";
        }
        ImGui::Separator();
        ImGui::TextDisabled("%s", consoleOutput.c_str());

        ImGui::End();
        //********************************************************************************
        // ImGui::Begin("Windows Options");
        // if (ImGui::BeginCombo("##Theme", selectedTheme.c_str()))
        // {
        //     if (ImGui::Selectable("Dark", selectedTheme == "Dark"))
        //     {
        //         ChangeTheme(style, backupStyle, "Dark");
        //         selectedTheme = "Dark";
        //     }
        //     if (ImGui::Selectable("Light", selectedTheme == "Light"))
        //     {
        //         ChangeTheme(style, backupStyle, "Light");
        //         selectedTheme = "Light";
        //     }
        //     if (ImGui::Selectable("Classic", selectedTheme == "Classic"))
        //     {
        //         ChangeTheme(style, backupStyle, "Classic");
        //         selectedTheme = "Classic";
        //     }
        //     if (ImGui::Selectable("XP", selectedTheme == "XP"))
        //     {
        //         ChangeTheme(style, backupStyle, "XP");
        //         selectedTheme = "XP";
        //     }
        //     if (ImGui::Selectable("VSModernDark", selectedTheme == "VSModernDark"))
        //     {
        //         ChangeTheme(style, backupStyle, "VSModernDark");
        //         selectedTheme = "VSModernDark";
        //     }
        //     ImGui::EndCombo();
        // }
        // ImGui::SameLine();
        // ImGui::End();

        // Rendering
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
        SDL_PumpEvents();
    }

    // Cleanup
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
