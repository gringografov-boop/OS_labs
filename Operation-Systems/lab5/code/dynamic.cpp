#include "dialog.h"
#include "os/os.h"
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace {
class Library {
#ifdef _WIN32
    HMODULE handle=nullptr;
#else
    void* handle=nullptr;
#endif
public:
    Function derivative=nullptr, square=nullptr;
    explicit Library(const std::string& path) {
#ifdef _WIN32
        const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,nullptr,0);
        if(!n) throw std::runtime_error("Invalid UTF-8 library path");
        std::wstring wide(static_cast<std::size_t>(n),L'\0');
        if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,wide.data(),n))
            throw std::runtime_error("Cannot convert library path");
        handle=LoadLibraryW(wide.c_str());
        if(!handle) throw std::runtime_error("LoadLibraryW failed: "+std::to_string(GetLastError()));
        derivative=reinterpret_cast<Function>(GetProcAddress(handle,"Derivative"));
        square=reinterpret_cast<Function>(GetProcAddress(handle,"Square"));
#else
        handle=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
        if(!handle) throw std::runtime_error(dlerror());
        derivative=reinterpret_cast<Function>(dlsym(handle,"Derivative"));
        square=reinterpret_cast<Function>(dlsym(handle,"Square"));
#endif
        if(!derivative || !square) {
            Close();
            throw std::runtime_error("Library does not export Derivative and Square");
        }
    }
    Library(const Library&)=delete;
    Library& operator=(const Library&)=delete;
    ~Library(){Close();}
private:
    void Close() noexcept {
        if(!handle) return;
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        handle=nullptr;
    }
};
std::string Path(int index) {
    const std::string exe=os::ExecutablePath();
    const auto slash=exe.find_last_of("/\\");
    if(slash==std::string::npos) throw std::runtime_error("Executable directory not found");
#ifdef _WIN32
    return exe.substr(0,slash+1)+"./lab5_impl"+std::to_string(index)+".dll";
#else
    return exe.substr(0,slash+1)+"./liblab5_impl"+std::to_string(index)+".so";
#endif
}
}
int main() {
    try {
        int current=1;
        auto library=std::make_unique<Library>(Path(current));
        Function derivative=library->derivative, square=library->square;
        return Dialogue(derivative,square,[&]{
            const int next=3-current;
            auto replacement=std::make_unique<Library>(Path(next));
            derivative=replacement->derivative;
            square=replacement->square;
            library.swap(replacement);
            current=next;
            std::cout<<"Implementation "<<current<<std::endl;
        });
    } catch(const std::exception& e) {
        std::cerr<<"lab5_dynamic: "<<e.what()<<'\n';
        return 1;
    }
}
