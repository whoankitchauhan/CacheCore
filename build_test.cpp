#include <optional>
#include <string>
int main() { std::optional<std::string> x = std::string("hi"); return x.has_value() ? 0 : 1; }
