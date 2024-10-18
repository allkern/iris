#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct instruction {
    std::string mnemonic;
    std::vector <std::string> args;
};

int main() {
    std::ifstream in("fmt.txt");
    std::ofstream out("fmt.c");

    std::string line;

    while (!in.eof()) {
        std::getline(in, line);
        std::istringstream ss(line);

        instruction i;

        while (!ss.eof()) {
            char c = ss.get();

            while (isalpha(c) || (c == '.') || isdigit(c)) {
                i.mnemonic.push_back(c);
                c = ss.get();
            }

            while (isspace(c))
                c = ss.get();

            if (ss.eof())
                break;

            while (!ss.eof()) {
                std::string arg;

                while ((!ss.eof()) && (c != ',')) {
                    arg.push_back(c);
                    c = ss.get();
                }

                c = ss.get();

                while (isspace(c))
                    c = ss.get();

                i.args.push_back(arg);
            }
        }

        char buf[512];
        char* ptr = buf;

        std::string fn, mnemonic;

        for (char c : i.mnemonic)
            if (c != '.')
                fn.push_back(tolower(c));

        for (char c : i.mnemonic)
            mnemonic.push_back(tolower(c));

        if (!i.args.size()) {
            ptr += sprintf(ptr, "static inline void ee_d_%s(uint32_t opcode) { ptr += sprintf(ptr, \"%%-8s\", \"%s\");", fn.c_str(), mnemonic.c_str());

            continue;
        }

        ptr += sprintf(ptr, "static inline void ee_d_%s(uint32_t opcode) { ptr += sprintf(ptr, \"%%-8s ", fn.c_str());

        for (const std::string& a : i.args) {
            if (a[0] == 'r') {
                ptr += sprintf(ptr, "%%s, ");
            } else if (a == "sa") {
                ptr += sprintf(ptr, "%%d, ");
            } else if (a == "target") {
                ptr += sprintf(ptr, "%%08x, ");
            } else if (a == "offset") {
                ptr += sprintf(ptr, "0x%%x, ");
            } else if (a == "immediate") {
                ptr += sprintf(ptr, "%%d, ");
            } else if (a == "offset(base)") {
                ptr += sprintf(ptr, "%%d(%%s), ");
            } else if (a[0] == 'f') {
                ptr += sprintf(ptr, "f%%d, ");
            }
        } 

        ptr -= 2;

        ptr += sprintf(ptr, "\", \"%s\", ", mnemonic.c_str());
    
        for (const std::string& a : i.args) {
            if (a[0] == 'r') {
                ptr += sprintf(ptr, "ee_cc_r[EE_D_R%c], ", toupper(a[1]));
            } else if (a == "sa") {
                ptr += sprintf(ptr, "EE_D_SA, ");
            } else if (a == "target") {
                ptr += sprintf(ptr, "s->pc + EE_D_SI26, ");
            } else if (a == "offset") {
                ptr += sprintf(ptr, "s->pc + EE_D_SI16, ");
            } else if (a == "immediate") {
                ptr += sprintf(ptr, "EE_D_I16, ");
            } else if (a == "offset(base)") {
                ptr += sprintf(ptr, "EE_D_I16, ee_cc_r[EE_D_RT], ", toupper(a[1]));
            } else if (a[0] == 'f') {
                ptr += sprintf(ptr, "ee_cc_r[EE_D_R%C], ", toupper(a[1]));
            }
        }

        ptr -= 2;

        ptr += sprintf(ptr, "); }");

        puts(buf);
    }
}