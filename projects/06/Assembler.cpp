#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <regex>
#include <bitset>
#include <map>

using namespace std;

enum class Command { A_COMMAND, C_COMMAND, L_COMMAND };

class Parser {
private:
    ifstream ifs;
    string word, symbol, dest, comp, jump;
    Command commandtype;
    smatch m;

    void NextWord() {
        static regex EX_COMMENT(R"(//.*)");
        while (true) {
            if (!getline(ifs, word)) return;
            word = regex_replace(word, EX_COMMENT, "");
            word.erase(remove(word.begin(), word.end(), ' '), word.end());
            if (word.size()) break;
        }
    }
public:
    Parser(string filename) {
        ifs.open(filename, ios::in);
        NextWord();
    }

    bool HasMoreCommands() { return !ifs.eof(); }

    void Advance() {
        static regex A_MATCH(R"(^@[\w\.\$:]+)");
        static regex L_MATCH(R"(^\([\w\.\$:]+\)$)");
        static regex C_MATCH(R"((A?M?D?=)?([01AMD&!|+\-]*)(;[A-Z]*)?)");

        if (regex_match(word, m, A_MATCH)) {
            commandtype = Command::A_COMMAND;
            symbol = m.str().substr(1);
        } else if (regex_match(word, m, L_MATCH)) {
            commandtype = Command::L_COMMAND;
            symbol = m.str().substr(1, m.str().size() - 2);
        } else {
            commandtype = Command::C_COMMAND;
            regex_match(word, m, C_MATCH);
            dest = m[1].str();
            if (dest.size()) dest = dest.substr(0, dest.size() - 1);
            comp = m[2].str();
            jump = m[3].str();
            if (jump.size()) jump = jump.substr(1);
        }

        NextWord();
    }

    Command CommandType() { return commandtype; }

    string Symbol() { return symbol; }

    string Dest() { return dest; }

    string Comp() { return comp; }

    string Jump() { return jump; }
};

class Code {
public:
    string Dest(string dest) {
        string res = "000";
        if (dest.find("A") != string::npos) res[0] = '1';
        if (dest.find("D") != string::npos) res[1] = '1';
        if (dest.find("M") != string::npos) res[2] = '1';
        return res;
    }

    string Comp(string comp) {
        if (comp == "0") return "0101010";
        if (comp == "1") return "0111111";
        if (comp == "-1") return "0111010";
        if (comp == "D") return "0001100";
        if (comp == "A") return "0110000";
        if (comp == "M") return "1110000";
        if (comp == "!D") return "0001101";
        if (comp == "!A") return "0110001";
        if (comp == "!M") return "1110001";
        if (comp == "-D") return "0001111";
        if (comp == "-A") return "0110011";
        if (comp == "-M") return "1110011";
        if (comp == "D+1") return "0011111";
        if (comp == "A+1") return "0110111";
        if (comp == "M+1") return "1110111";
        if (comp == "D-1") return "0001110";
        if (comp == "A-1") return "0110010";
        if (comp == "M-1") return "1110010";
        if (comp == "D+A" || comp == "A+D") return "0000010";
        if (comp == "D+M" || comp == "M+D") return "1000010";
        if (comp == "D-A") return "0010011";
        if (comp == "D-M") return "1010011";
        if (comp == "A-D") return "0000111";
        if (comp == "M-D") return "1000111";
        if (comp == "D&A" || comp == "A&D") return "0000000";
        if (comp == "D&M" || comp == "M&D") return "1000000";
        if (comp == "D|A" || comp == "A|D") return "0010101";
        if (comp == "D|M" || comp == "M|D") return "1010101";
    }

    string Jump(string jump) {
        if (jump == "JGT") return "001";
        if (jump == "JEQ") return "010";
        if (jump == "JGE") return "011";
        if (jump == "JLT") return "100";
        if (jump == "JNE") return "101";
        if (jump == "JLE") return "110";
        if (jump == "JMP") return "111";
        return "000";
    }
};

class SymbolTable {
private:
    map<string, int> mp;
public:
    SymbolTable() {
        mp.emplace("SP", 0);
        mp.emplace("LCL", 1);
        mp.emplace("ARG", 2);
        mp.emplace("THIS", 3);
        mp.emplace("THAT", 4);
        mp.emplace("SCREEN", 16384);
        mp.emplace("KBD", 24576);
        for (int i = 0; i < 16; ++i) mp.emplace("R" + to_string(i), i);
    }

    void AddEntry(string symbol, int address) { mp.emplace(symbol, address); }

    bool Contains(string symbol) { return mp.find(symbol) != mp.end(); }

    int GetAddress(string symbol) { return mp[symbol]; }
};

int main(int argc, char** argv) {
    string filename = argv[1];

    Parser ps(filename);
    Code cd;
    SymbolTable st;

    int address = 0;
    while (ps.HasMoreCommands()) {
        ps.Advance();
        if (ps.CommandType() == Command::L_COMMAND) {
            st.AddEntry(ps.Symbol(), address);
            --address;
        }
        ++address;
    }

    ofstream ofs(filename.substr(0, filename.size() - 3) + "hack");

    ps = Parser(filename);
    int ram = 16;
    while (ps.HasMoreCommands()) {
        ps.Advance();
        if (ps.CommandType() == Command::L_COMMAND) continue;
        string res;
        if (ps.CommandType() == Command::A_COMMAND) {
            string symbol = ps.Symbol();
            unsigned int num = 0;
            if (all_of(symbol.begin(), symbol.end(), [](char c) { return isdigit(c); })) num = stoi(symbol);
            else {
                if (!st.Contains(symbol)) st.AddEntry(symbol, ram++);
                num = st.GetAddress(symbol);
            }
            bitset<15> bs(num);
            res = "0" + bs.to_string();
        } else {
            string comp = cd.Comp(ps.Comp());
            string dest = cd.Dest(ps.Dest());
            string jump = cd.Jump(ps.Jump());
            res = "111" + comp + dest + jump;
        }
        ofs << res << endl;
    }


    return 0;
}