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

string _getFirstArg(const char* cmd_line) {
  string cmd_s = _trim(string(cmd_line));
  return cmd_s.substr(0, cmd_s.find_first_of(" \n"));
}

string _getSecondArg(const char* cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    
    size_t firstSpace = cmd_s.find(' ');
    return firstSpace == string::npos ? "" : cmd_s.substr(firstSpace + 1);
}

string _getThirdArg(const char* cmd_line, const char* secondArg) {
    string cmd_s = _trim(string(cmd_line));
    
    size_t firstSpace = cmd_s.find(' ');
    if (firstSpace == string::npos) return "";

    size_t secondSpace = cmd_s.find(' ', firstSpace + 1);
    if (secondSpace == string::npos) return "";

    return cmd_s.substr(secondSpace + 1);
}

bool _isPositiveInteger(const std::string& str) {
    if (str.empty()) return false;

    for (char c : str) {
        if (!isdigit(c)) {
            return false;
        }
    }
    return true;
}

bool _isNegativeInteger(const std::string& str) {
  if (str.empty() || str.length() == 1) {
    return false;
  }
  if (str[0] != '-') {
    return false;
  }
  for (size_t i = 1; i < str.length(); ++i) {
    if (!isdigit(str[i])) {
      return false;  // If a non-digit character is found, return false
    }
  }
  return true;
}

Command::Command(const char *cmd_line){
  this->cmd_line = cmd_line;
}

std::string Command::getLine()
{
    return this->cmd_line;
}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
  this->cmd_line = cmd_line;
}

CHPromptCommand::CHPromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
  string newPrompt = _getSecondArg(cmd_line);
  this->newPromptName =  newPrompt;
}

void CHPromptCommand::execute() {
  SmallShell::getInstance().changePrompt(this->newPromptName);
}
// TODO: Add your implementation for classes in Commands.h 
void SmallShell::changePrompt(const string newPrompt) {
  this->promptName = newPrompt == "" ? "smash> " : newPrompt + "> ";
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

JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line) {
  this->cmd_line = cmd_line;
  this->jobs = jobs;
}

void JobsCommand::execute() {
  this->jobs->printJobsList();
}

FGCommand::FGCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
  this->cmd_line = cmd_line;
}

void FGCommand::execute() {
  int ID;
  string jobID = _getSecondArg(this->cmd_line.c_str());
  if (jobID.compare("") && _isPositiveInteger(jobID)) { // need to check for third argument
    ID =  stoi(jobID);
  }
  else if (SmallShell::getInstance().getJobs()->isEmpty()){
    std::cout << "smash error: fg: jobs list is empty" << std::endl;
    return;
  }
  else {
    ID = SmallShell::getInstance().getJobs()->getLastJobID();
  }

  JobsList::JobEntry* job = SmallShell::getInstance().getJobs()->getJobById(ID);

  if (!job) {
    std::cout << "smash error: fg: job-id " << ID << " does not exist" << std::endl;
    return;
  }
  
  //probably need to change
  int pid = fork();
  if (pid == 0) { //child
  std::cout <<  "im a child";
  //execute the task
  }
  else {          //father
  std::cout << pid << std::endl; //should also print the proccess line but we didnt implement the functions yet
  waitpid(pid, NULL, WUNTRACED); //not sure about the flag
  }
  return;
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line) {
  this->cmd_line = cmd_line;
  this->jobs = jobs;
}

void QuitCommand::execute() {
  string secondArg = _getSecondArg(this->cmd_line.c_str());
  if (!secondArg.compare("kill")) {
    std::cout << "sending SIGKILL signal to " << jobs->jobs.size() << " jobs:";

    //need to print the pid number as well
    //also need to kill the jobs

  for (auto it = jobs->jobs.begin(); it != jobs->jobs.end();) {
    std::cout << it->getName() << " " << it->getFullCommandArgs() << std::endl;
  }
  jobs->killAllJobs();
  }
  exit(0);
}

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line) {
  this->cmd_line = cmd_line;
  this->jobs = jobs;
}

void KillCommand::execute() {
  string secondArg = _getSecondArg(this->cmd_line.c_str());
  string thirdArg = _getThirdArg(this->cmd_line.c_str(), secondArg.c_str());
//only 1-2 args or not valid numbers or not a valid signal

  if ((!secondArg.compare("kill") || !thirdArg.compare("kill")) &&
      (_isNegativeInteger(secondArg) || !_isPositiveInteger(thirdArg)) &&
      ((stoi(secondArg) > -1) || (stoi(secondArg) < -32))) {
        std::cout << "smash error: kill: invalid arguments" << std::endl;
    return;
  }
  int ID = stoi(thirdArg);
  JobsList::JobEntry* job = jobs->getJobById(ID);

  if (!job) {
    std::cout << "smash error: kill: job-id " << ID << " does not exist" << std::endl;
    return;
  }

  //need to kill job when adding the function

  jobs->removeJobById(stoi(thirdArg));
}

bool JobsList::isEmpty() {
  return this->jobs.empty();
}

JobsList::JobsList() {
  this->nextJobID = 1;
}

JobsList::JobEntry::JobEntry(int ID, const char* name, const char* fullCommandArgs, bool isStopped) {
  this->ID = ID;
  this->name = name;
  this->fullCommandArgs = fullCommandArgs;
  this->isActive = isStopped;
}

void JobsList::addJob(Command *cmd, bool isStopped) {
  removeFinishedJobs();
  //implement decifering the job and entering the details - later
  string name = "bruh";
  string fullCommandArgs = "yeah boi";
  this->jobs.emplace_back(nextJobID++, name.c_str(), fullCommandArgs.c_str(), isStopped);
}

void JobsList::printJobsList() {
  removeFinishedJobs();
  for (const auto& job : jobs) {
    job.printEntry();
  }
}

JobsList* SmallShell::getJobs() {
  return this->jobs;
}

void JobsList::JobEntry::printEntry() const {
  std::cout << "[" << this->ID << "] " << this->name << " " << this->fullCommandArgs;
}

void JobsList::killAllJobs() {
  // also need to actually kill them
  for (auto& job : jobs) {
    job.isActive = false;
  }
  jobs.clear();
}

//fill in
void JobsList::removeFinishedJobs() {
  for (auto it = jobs.begin(); it != jobs.end(); ) {
    if (!it->isActive) {  // If job is not active
      it = jobs.erase(it);  // Remove job and get the new iterator
    }
    else {
      ++it;  // Otherwise, just move to the next job
    }
  }
}

JobsList::JobEntry *JobsList::getJobById(int jobId){
  for (auto& job : jobs) {
    if (job.equalsToID(jobId)) {
      return &job;  // Return the address of the job if found
    }
  }
  return nullptr;  // Return nullptr if no job with the given ID is found
}

bool JobsList::JobEntry::equalsToID(int jobId) {
  return this->ID == jobId;
}

void JobsList::removeJobById(int jobId) {
  for (auto it = jobs.begin(); it != jobs.end(); ++it) {
    if (it->equalsToID(jobId)) {
      jobs.erase(it);
      break;
    }
  }
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId){
  if (jobs.empty()) {
    *lastJobId = -1;
    return nullptr;
  }
  *lastJobId = jobs.back().getID();
  return &jobs.back();
}

int JobsList::getLastJobID(){
  return this->jobs.back().getID();
}

int JobsList::JobEntry::getID(){
  return this->ID;
}

string JobsList::JobEntry::getName()
{
    return this->name;
}
string JobsList::JobEntry::getFullCommandArgs()
{
    return this->fullCommandArgs;
}

JobsList::JobEntry *SmallShell::head()
{
  if (!this->jobs->jobs.empty()) {
    return &this->jobs->jobs.front();
  }
  return nullptr;
}

JobsList::JobEntry *SmallShell::tail()
{
  if (!this->jobs->jobs.empty()) {
    return &this->jobs->jobs.back();
  }
  return nullptr;}

int JobsList::zombieTheJob(int jobId) {
  JobEntry* job = getJobById(jobId);
  if(job) {
    job->isActive = false;
    return 0;
  }
  return -1;
}

SmallShell::SmallShell() {
// TODO: add your implementation
  this->promptName = "smash> ";
  this->currPwd = getcwd(nullptr, 0);
  this->jobs = new JobsList();
}

SmallShell::~SmallShell() {
// TODO: add your implementation
  delete jobs;
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
  string newDir = _getSecondArg(cmd_line);
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
  string firstWord = _getFirstArg(cmd_line);

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
    return new JobsCommand(cmd_line, jobs);
  }
  else if (firstWord.compare("fg") == 0) {
    return new FGCommand(cmd_line);
  }
  else if (firstWord.compare("quit") == 0) {
    return new QuitCommand(cmd_line, this->jobs);
  }
  else if (firstWord.compare("kill") == 0) {
    return new KillCommand(cmd_line, this->jobs);
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
