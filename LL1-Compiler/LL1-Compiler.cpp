﻿#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <algorithm>
#include <string>
#include <fstream>

#include "constants.cpp"
#include "helpers.cpp"
#include "scanner.cpp"
#include "LL1.cpp"
#include "IR.cpp"
#include "asm.cpp"

using std::string;
using std::vector;
using std::map;
using std::set;
using std::pair;
using std::stack;
using std::cout;
using std::ofstream;


bool checkLine(vector<vector<string>> productionTable, map<string, map<string, int>> parseTable, string line, Node*& root, variable& var, vector<variable>& variables, vector<string> labels, string& output) {
	vector<string> stack;
	string focus;
	Node* focusNode = root;
	vector<int> depthStack;

	bool isNegVal = true;
	string word = nextWord(line, isNegVal);
	if (line[0] == ' ')
		line.erase(0, 1);
	line.erase(0, word.size());
	stack.push_back("eof");
	stack.push_back("GOAL");
	focus = stack.back();

	while (true) {

		if (focus == "eof" && word == "eof") {
			// then report successand exit the loop;
			return 1;
		}
		else if (isTerminal(focus) || focus == "eof") {
			if (focus == getTermType(word)) {
				stack.pop_back();

				// If type declaring for a variable
				if (stack.back() == "VARTYPEAFTER") {
					var.type = word;
				}
				else if (word == "procedure" || word == "return") {
					var.type = word;
				}
				else if (stack.back() == "LINEVARNAMEREMAINING") {
					for (variable varLook : variables) {
						if (varLook.name == word)
							return 0; // Duplicate variable name error
					}
					var.name = word;
				}

				// If not elipson
				else if (word != "e" && word != "=" && word != "(" && word != ")" && word != "{" && word != "}" && word != ",") {
					addToTree(root, focusNode, word, variables);
				}

				// Create Empty Node on tree
				if (isRONT(stack.back())) {
					focusNode->child2 = new Node();
					focusNode->child2->parent = focusNode;
					focusNode = focusNode->child2;

					depthStack.push_back(stack.size());
				}

				// If word is name for variable

				string nextNewWord = nextWord(line, isNegVal);
				if (line[0] == ' ')
					line.erase(0, 1);

				line.erase(0, nextNewWord.size());

				while (nextNewWord[0] == ' ')
					nextNewWord.erase(0, 1);

				// Gets procedure name and procedure params
				if (focus == "name" && nextNewWord == "(") {
					// insert values in eax and ebx and call label
					string eaxReg = line.substr(0, line.find(','));
					line.erase(0, eaxReg.size() + 1);
					string ebxReg = "0";
					ebxReg = line.substr(0, line.find(')'));
					line.erase(0, eaxReg.size() + 2);

					removeBeginSpace(eaxReg);
					removeBeginSpace(ebxReg);

					output += movValInReg(eaxReg);
					output += movValInReg(ebxReg);
					output += callFunction(word);

					if (line == "")
						return 1;

					nextNewWord = nextWord(line, isNegVal);
					if (line[0] == ' ')
						line.erase(0, 1);

					line.erase(0, nextNewWord.size());

					while (nextNewWord[0] == ' ')
						nextNewWord.erase(0, 1);

					// TODO: store list of labels and store that in the tree like a varName
					// - maybe store in a special node type that holds param values
					// - then make un optimized
					// - in asm look through labelnames like how varNames are done
				}

				word = nextNewWord;

			}
			else {
				// else report an error looking for symbol at top of stack;
				return 0;
			}
		}
		else {
			try {
				// focus is a nonterminal
				string termType = getTermType(word);
				int productionNum = parseTable[focus][termType];

				if (productionNum == -1 || termType == "error")
					return 0;

				vector<string> curProduction = productionTable[productionNum];

				if (!curProduction.empty()) {
					if (stack.back() == curProduction.front())
						return 0;

					// If consuming a RONT, move up the tree
					if (stack.size() > 0 && depthStack.size() > 0 && focusNode != nullptr) {
						if (curProduction.front() == "e" && !isOperator(stack.back()) && !isRONT(stack.back())) {
							if (depthStack.back() == stack.size()) {
								depthStack.pop_back();
								focusNode = focusNode->parent;
							}
						}
					}

					stack.pop_back();

					for (int i = curProduction.size() - 1; i >= 0; i--) {
						if (curProduction[i] != "e") {
							stack.push_back(curProduction[i]);
						}
					}

				}
				else {
					// report an error expanding focus;
					return 0;
				}
			}
			catch (int ex) {
				return 0;
			}
		}

		string nameNumList[] = { "name", "num", "spacenegname", "spacenegnum", "negname", "negnum", "(" };
		isNegVal = !isInArray(nameNumList, getTermType(word)) && word != ")"; // lastWordWasANumberVarOrRightparens 

		focus = stack.back();
	}
}

int main()
{
	// Read in files 
	vector<string> file = readInFile("./asm.txt");

	// Create structures for the algorithm
	vector<vector<string>> productionTable = createProductionTable();
	map<string, map<string, int>> parseTable = createParseTable(productionTable);

	// Keep track of the IR
	vector <string> labels;
	vector < variable > variables;
	stack <vector<variable> > scope;

	string procName = "";
	

	// Keeps track of the main body asm output
	string asmBody = "";

	for (int i = 0; i < file.size(); i++) {
		string line = file[i];
		bool isCheck = true;

		// Skip lines with comments
		if (line[0] == '/' && line[1] == '/') {
			cout << "\n\n" << line << "\n";
			continue;
		}

		string passedStr = "invalid";
		string errorStr = "";

		// Keeps track of the values in a tree
		Node* root = nullptr;
		variable var;

		// If first word is a print function; get asm for print
		if (line.find("printString") != string::npos || line.find("printNum") != string::npos) {
			asmBody += getPrintString(line, variables);
			isCheck = false;
			passedStr = "valid";
		}

		if (line.find("readNum") != string::npos) {
			// If readNum is found, output asm for it
			string lineCopy = line;
			asmBody += readNum(lineCopy.erase(0, 8));

			// Set var to be unOptimized
			for (int i = 0; i < variables.size(); i++) {
				variable var = variables[i];
				if (var.name == lineCopy) {
					var.isOptimized = false;
				}
			}


			isCheck = false;
			passedStr = "valid";
		}

		// If procedure is in line; create label point here
		if (line.find("procedure") != string::npos) {
			string lineCopy = line;
			lineCopy.erase(0, 14);
			procName = lineCopy.substr(0, getStrPos(lineCopy, '('));
			labels.push_back(procName);

			asmBody += asmSetProcedureLabel(procName);

			isCheck = false;
			passedStr = "valid";
		}

		if (line.find("return") != string::npos) {
			string lineCopy = line;
			lineCopy.erase(0, 7);
			asmBody += asmReturn(procName, lineCopy);
			isCheck = false;
			passedStr = "valid";
		}

		// If '{' is in the line; Push scope to stack and start new scope
		if (isInString(line, '{')) {
			// Create scope here // Use scope variable here
			scope.push(variables);
			variables.clear();
			asmBody += asmPushStack();

			isCheck = false;
			passedStr = "valid";
		}

		// If '}' is in line; Pop scope off stack and continue to use that scope
		if (isInString(line, '}')) {
			// End scope here // Use scope variable here
			variables.clear();
			variables = scope.top();
			scope.pop();
			// asmBody += asmPopStack();

			isCheck = false;
			passedStr = "valid";
		}

		// Perform the algorithm
		if (isCheck && checkLine(productionTable, parseTable, line, root, var, variables, labels, asmBody)) {
			try {
				passedStr = "valid";

				// Perform Math operation on line using post-order traversal and save into variables
				var.value = performCalc(root, var.type, variables, labels, asmBody);
				var.isOptimized = true;

				if (i < file.size() && file[i+1].find("readNum") != string::npos) {
					var.isOptimized = false;
				}

				// Add asm output for optimized line
				asmBody += "\n; " + line + " => Optimized";
				asmBody += operationEq(var);

			}
			catch (std::invalid_argument e) { passedStr = "invalid"; }
			catch (std::exception e) {
				// Make certain vars wait for runtime to perform operation
				// - post order traversal on root node but with outputing asm instead
				try {
					var.isOptimized = false;
					asmBody += "\n; " + line;
					getOpString(root, asmBody, var.name, labels); // TODO: check to make sure all times it comes in here is when we want it to
				}
				catch (std::exception e) {
					passedStr = "invalid";
				}
			}

			var.root = root;
			variables.push_back(var);
		}

		cout << "(" << passedStr << "): " << line << "\n";
	}

	cout << "\n\nVariables\n";
	for (variable var : variables) {
		if (var.type == "num")
			cout << var.type << " " << var.name << " = " << (int)var.value << "\n";
		else if (var.type == "ish")
			cout << var.type << " " << var.name << " = " << var.value << "\n";
	}

	writeToFile(asmBody, startFile(variables));


	return 0;
}
