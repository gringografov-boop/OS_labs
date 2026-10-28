template <typename... Args>
std::string err::CreateReport(const char* file, int line, const char* condition, const char* fmt, const Args... args)
{
    std::stringstream stream;
    std::string buff;
    std::sprintf(buff.data(), "==================================================================\n");
    stream << buff;
    std::sprintf(buff.data(), "ASSERTION AT %s: %d", file, line);
    stream << buff;
    std::sprintf(buff.data(), "Condition: %s", condition);
    stream << buff;
    std::sprintf(buff.data(), "Message: %s", fmt, args...);
    return stream.str();
}