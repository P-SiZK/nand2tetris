#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <regex>
#include <bitset>
#include <map>

using namespace std;

enum class Command {
	C_ARITHMETIC, C_PUSH, C_POP,
	C_LABEL, C_GOTO, C_IF,
	C_FUNCTION, C_RETURN, C_CALL
};

class Parser {
private:
	ifstream ifs;
	string word, arg1;
	Command commandtype;
	int arg2;
	smatch m;

	void NextWord() {
		static regex EX_FSPACE(R"(^\s+)");
		static regex EX_COMMENT(R"(//.*)");
		while (true) {
			if (!getline(ifs, word)) return;
			word = regex_replace(word, EX_FSPACE, "");
			word = regex_replace(word, EX_COMMENT, "");
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
		static regex MATCH_EXPR(R"(^(\S+)\s*(\S+)?\s*(\d+)?\s*$)");
		regex_match(word, m, MATCH_EXPR);
		string ct = m[1].str();
		commandtype = [&]() {
			if (ct == "push") return Command::C_PUSH;
			if (ct == "pop") return Command::C_POP;
			if (ct == "label") return Command::C_LABEL;
			if (ct == "goto") return Command::C_GOTO;
			if (ct == "if-goto") return Command::C_IF;
			if (ct == "function") return Command::C_FUNCTION;
			if (ct == "call") return Command::C_CALL;
			if (ct == "return") return Command::C_RETURN;
			return Command::C_ARITHMETIC;
		}();
		arg1 = (commandtype == Command::C_ARITHMETIC ? ct : m[2].str());
		if (m[3].str().size()) arg2 = stoi(m[3].str());
		NextWord();
	}

	Command CommandType() { return commandtype; }

	string Arg1() { return arg1; }

	int Arg2() { return arg2; }
};

class CodeWriter {
private:
	ofstream ofs;
	string filename;
	int arithmeticnum = 0;
	smatch m;

	void PushDToStack() {
		ofs << "@SP" << endl
			<< "M=M+1" << endl
			<< "A=M-1" << endl
			<< "M=D" << endl;
	}

	void PopDFromStack() {
		ofs << "@SP" << endl
			<< "M=M-1" << endl
			<< "A=M" << endl
			<< "D=M" << endl;
	}
public:
	CodeWriter(string filename) {
		if (filename.substr(filename.size() - 3) == ".vm") {
			filename = filename.substr(0, filename.size() - 3);
		} else {
			if (filename.back() != '/') filename.push_back('/');
			regex_search(filename, m, regex(R"(([^/]+)/$)"));
			filename += m[1].str();
		}

		ofs.open(filename + ".asm");
	}

	void SetFileName(string filename) {
		static regex EX_FILENAME(R"([^/]+$)");

		regex_search(filename, m, EX_FILENAME);
		this->filename = m.str();
	}

	void WriteArithmetic(string command) {
		ofs << "@SP" << endl;
		if (command == "neg" || command == "not") {
			ofs << "D=M-1" << endl
				<< "A=D" << endl
				<< (command == "neg" ? "M=-M" : "M=!M") << endl;
		} else {
			ofs << "M=M-1" << endl
				<< "A=M" << endl
				<< "D=M" << endl
				<< "A=A-1" << endl;
			if (command == "eq" || command == "gt" || command == "lt") {
				ofs << "D=M-D" << endl
					<< "@$ARITHMETIC_IF_" << arithmeticnum << "$" << endl;
				ofs << [&]() {
					if (command == "eq") return "D;JEQ";
					if (command == "gt") return "D;JGT";
					return "D;JLT";
				}() << endl;
				ofs << "@SP" << endl
					<< "A=M-1" << endl
					<< "M=0" << endl
					<< "@$ARITHMETIC_ENDIF_" << arithmeticnum << "$" << endl
					<< "0;JMP" << endl
					<< "($ARITHMETIC_IF_" << arithmeticnum << "$)" << endl
					<< "@SP" << endl
					<< "A=M-1" << endl
					<< "M=-1" << endl
					<< "($ARITHMETIC_ENDIF_" << arithmeticnum << "$)" << endl;
				++arithmeticnum;
			} else {
				ofs << [&]() {
					if (command == "add") return "M=M+D";
					if (command == "sub") return "M=M-D";
					if (command == "and") return "M=M&D";
					return "M=M|D";
				}() << endl;
			}
		}
	}

	void WritePushPop(Command command, string segment, int index) {
		if (command == Command::C_PUSH) {
			if (segment == "static") {
				ofs << "@" << filename << "." << index << endl
					<< "D=M" << endl;
			} else if (segment == "pointer" || segment == "temp") {
				ofs << "@" << index + (segment == "pointer" ? 3 : 5) << endl;
				ofs << "D=M" << endl;
			} else {
				ofs << "@" << index << endl
					<< "D=A" << endl;
				if (segment != "constant") {
					string symbol = [&]() {
						if (segment == "local") return "@LCL";
						if (segment == "argument") return "@ARG";
						if (segment == "this") return "@THIS";
						return "@THAT";
					}();
					ofs << symbol << endl;
					ofs << "A=M+D" << endl
						<< "D=M" << endl;
				}
			}
			PushDToStack();
		} else {
			if (segment == "static") {
				PopDFromStack();
				ofs << "@" << filename << "." << index << endl
					<< "M=D" << endl;
			} else if (segment == "pointer" || segment == "temp") {
				PopDFromStack();
				ofs << "@" << index + (segment == "pointer" ? 3 : 5) << endl
					<< "M=D" << endl;
			} else {
				string symbol = [&]() {
					if (segment == "local") return "@LCL";
					if (segment == "argument") return "@ARG";
					if (segment == "this") return "@THIS";
					return "@THAT";
				}();
				ofs << "@" << index << endl
					<< "D=A" << endl
					<< symbol << endl
					<< "M=M+D" << endl;
				PopDFromStack();
				ofs << symbol << endl
					<< "A=M" << endl
					<< "M=D" << endl
					<< "@" << index << endl
					<< "D=A" << endl
					<< symbol << endl
					<< "M=M-D" << endl;
			}
		}
	}

	void close() { ofs.close(); }
};

int main(int argc, char** argv) {
	string filename = argv[1];

	Parser ps(filename);
	CodeWriter cw(filename);
	cw.SetFileName(filename);

	while (ps.HasMoreCommands()) {
		ps.Advance();
		switch (ps.CommandType()) {
		case Command::C_ARITHMETIC:
			cw.WriteArithmetic(ps.Arg1());
			break;
		case Command::C_PUSH:
		case Command::C_POP:
			cw.WritePushPop(ps.CommandType(), ps.Arg1(), ps.Arg2());
			break;
		}
	}
	cw.close();

	return 0;
}