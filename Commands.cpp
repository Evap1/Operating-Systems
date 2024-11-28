#include <unistd.h>
#include <string.h>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <iostream>
#include <unistd.h>
#include <cerrno>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

Command::Command(const char *cmd_line){
  this->cmd_line = cmd_line;
}


BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
  this->cmd_line = cmd_line;
}

CHPromptCommand::CHPromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  string newPrompt = cmd_s.substr(cmd_s.find_first_not_of(firstWord) + 1, cmd_s.find_first_of(" \n"));

  this->newPromptName =  newPrompt;
}

void CHPromptCommand::execute() {
  SmallShell::getInstance().changePrompt(this->newPromptName);
}
// TODO: Add your implementation for classes in Commands.h 
void SmallShell::changePrompt(const string newPrompt) {
  this->promptName = newPrompt == "chprompt" ? "smash> " : newPrompt + "> ";
}

string SmallShell::getPrompt() {
  return this->promptName;
}

string SmallShell:: getCurrentDirectory() {
  return this->currPwd;
}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
  this->cmd_line = cmd_line;
}

void GetCurrDirCommand::execute() {
  std::cout << SmallShell::getInstance().getCurrentDirectory() << std::endl;
}

GetJobsCommand::GetJobsCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
  this->cmd_line = cmd_line;
}

void GetJobsCommand::execute() {
  //std::cout << SmallShell::getInstance().getCurrentDirectory() std::endl;
}

JobsList::JobsList() {
  this->head = nullptr;
}

JobsList::JobEntry::JobEntry(int ID, const char* name, const char* fullCommandArgs) {
  this->ID = ID;
  this->name = name;
  this->fullCommandArgs = fullCommandArgs;
  this->next = nullptr;
}

void JobsList::addJob(Command *cmd, bool isStopped = false) {
  //implement decifering the job and entering the details - later
  string name = "bruh";
  string fullCommandArgs = "yeah boi";
  JobsList::JobEntry* newEntry = new JobsList::JobEntry(++(this->numberOfJobs), name.c_str(), fullCommandArgs.c_str());

  if (!head) {                        //no jobs in list
    head = newEntry;
    tail = newEntry;
  }
  else {
    this->tail->next = newEntry; //connect new entry
    this->tail = newEntry;       //set the new entry as the last on the list for easy access to next
  }
}

void JobsList::printJobsList() {
  JobEntry* entry = head;
  while (entry) {
    entry->printEntry();
    entry = entry->next;
  }
}

void JobsList::JobEntry::printEntry() {
  std::cout << "[" << this->ID << "] " << this->name << " " << this->fullCommandArgs;
}

void JobsList::killAllJobs() {
  JobEntry* current = head;
  while(current){
    JobEntry* deleteMe = current;
    current = current->next;
    delete current;                           //idk if its the right syntax
  }
}

SmallShell::SmallShell() {
// TODO: add your implementation
  this->promptName = "smash> ";
  this->currPwd = getcwd(nullptr, 0);
  this->jobs = new JobsList();
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}


/**
* ShowPID
*/
ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
  this->pid = getpid();
}

void ShowPidCommand::execute() {
  std::cout << "smash pid is " << this->pid << std::endl;
}


/**
* ChangeDirCommand
*/
ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line){
  //this->last_pwd = plastPwd;
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  string newDir = cmd_s.substr(cmd_s.find_first_not_of(firstWord) + 1, cmd_s.find_first_of(" \n"));

  this->dir_to_repl =  newDir;
}

void ChangeDirCommand::execute(){
  if (this->dir_to_repl.find_first_of(" ") != string::npos){
    std::cerr << "smash error: cd: too many arguments" << std::endl;
  }
  else if (this->dir_to_repl.compare("-") == 0){
    if (this->last_pwd == nullptr){
      std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
    }
    else{
      SmallShell::getInstance().setCurrentDirectory(this->dir_to_repl);
      this->last_pwd = SmallShell::getInstance().getLastDirectory();
    }

  }
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    // For example:

  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("chprompt") == 0) {
    return new CHPromptCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line, nullptr);
  }
  else if (firstWord.compare("jobs") == 0) {
    return new GetJobsCommand(cmd_line);
  }

  /*else {
    return new ExternalCommand(cmd_line);
  }*/

    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command* cmd = CreateCommand(cmd_line);
    cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}

void SmallShell::setCurrentDirectory(const std::string newDir){
  string tempLastPwd = this->currPwd;
  if (chdir(newDir.c_str()) != 0){
    // error
    std::perror("smash error: chdir failed");
  }
  else{
    this->currPwd = getcwd(nullptr, 0);
    this->lastPwd = tempLastPwd;
  }
}