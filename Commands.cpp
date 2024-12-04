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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <regex>

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

// return string without whispaces at the start and end.
string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

// return number of arfuments in command line
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

void _argsFree(int args_count, char **args) {
    if (!args)
        return;

    for (int i = 0; i < args_count; i++) {
        free(args[i]);
    }
}

// return true is it is a background command
bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}
// remove & from cmd_line 
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

bool isRedirection(int numOfArgs, char** args) {                                 // find if redirection command
  for (int i = 0; i < numOfArgs; i++) {
    if (!strcmp(args[i], ">") || !strcmp(args[i], ">>")) {
      return true;
    }
  }
  return false;
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
  return secondSpace == string::npos ? "" : cmd_s.substr(secondSpace + 1);
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

bool _isSingal(const std::string& str) {
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
  if (-stoi(str) < 0 || -stoi(str) > 32) {
    return false;
  }
  return true;
}

bool _moreThanXArgs(const std::string& str, unsigned int X) {
  return (str.length() > X) ? true : false;
}

bool isInPATHEnvVar(const char* arg) {
  char *pathEnv = getenv("PATH");
  if (!pathEnv) {
    perror("smash error: getenv failed");
    return false;
  }

  std::istringstream pathStream(pathEnv);
  std::string pathDir;

  while (std::getline(pathStream, pathDir, ':')) {
    
    std::string fullPath = pathDir + "/" + arg;    // construct the full path to the command

    if (access(fullPath.c_str(), X_OK) == 0) {     // check if the file exists and is executable
      return true;
    }
  }
  return false;
}

bool containsWildcards(char** args) {
  for (int i = 1; i < COMMAND_MAX_ARGS; ++i) {
    if (!args[i]) return false;
    else if (args[i][0] == '*' || args[i][0] == '?') { 
      return true;
    }
  }
  return false;  // No wildcards found
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
  string thirdArg = _getThirdArg(this->cmd_line.c_str(), jobID.c_str());
  if (!thirdArg.empty()) {
    std::cerr << "smash error: fg: invalid arguments" << std::endl;
  }
  else if (jobID.compare("") && _isPositiveInteger(jobID)) { // need to check for third argument
    ID =  stoi(jobID);
  }
  else if (SmallShell::getInstance().getJobs()->isEmpty()){
    std::cerr << "smash error: fg: jobs list is empty" << std::endl;
    return;
  }
  else {
    ID = SmallShell::getInstance().getJobs()->getLastJobID();
  }

  JobsList::JobEntry* job = SmallShell::getInstance().getJobs()->getJobById(ID);

  if (!job) {
    std::cerr << "smash error: fg: job-id " << ID << " does not exist" << std::endl;
    return;
  }
  else {
    std::cout << job->getCommandLine() << " " << job->getPID() << std::endl;
    job->isActive = false;
    waitpid(job->getPID(), NULL, 0);
    SmallShell::getInstance().getJobs()->removeJobById(ID);
  }
  return;
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line) {
  this->cmd_line = cmd_line;
  this->jobs = jobs;
}

void QuitCommand::execute() {
  this->jobs->removeFinishedJobs();

  string secondArg = _getSecondArg(this->cmd_line.c_str());
  if (!secondArg.compare("kill")) {
    std::cout << "smash: sending SIGKILL signal to " << jobs->jobs.size() << " jobs:" << std::endl;

    for (auto it = jobs->jobs.begin(); it != jobs->jobs.end();) {
      std::cout << it->getPID() << ": "<< it->getPID() << " "<< it->getCommandLine() << std::endl;
      kill(it->getPID(), SIGKILL);
      it->isActive = false;
      it = jobs->jobs.erase(it);
    }
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
  if ((!secondArg.compare("") || !thirdArg.compare("")) &&
      (_isSingal(secondArg) || !_isPositiveInteger(thirdArg))) {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
    return;
  }
  int ID = stoi(thirdArg);
  JobsList::JobEntry* job = jobs->getJobById(ID);

  if (!job) {
    std::cerr << "smash error: kill: job-id " << ID << " does not exist" << std::endl;
    return;
  }
  //need to kill job when adding the function
  int signal = abs(stoi(secondArg));
  std::cout << "signal number " << signal <<" was sent to pid " << job->getPID() << std::endl;
  kill(job->getPID(), signal);

  if (signal == SIGKILL) {
    job->isActive = false;
  }

  jobs->removeFinishedJobs();
}

bool JobsList::isEmpty() {
  return this->jobs.empty();
}

JobsList::JobsList() {
  this->nextJobID = 1;
}

JobsList::JobEntry::JobEntry(int ID,pid_t pid, Command* command, bool isActive) {
  this->ID = ID;
  this->pid = pid;
  this->command = command;
  this->isActive = isActive;
}

void JobsList::addJob(Command *cmd, pid_t pid, bool isActive) {
  removeFinishedJobs();
  this->jobs.emplace_back(nextJobID++, pid, cmd, isActive);
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
  std::cout << "[" << this->ID << "] " << this->command->getLine() << std::endl;
}

void JobsList::killAllJobs() {
  for (auto& job : jobs) {
    job.isActive = false;
    std::cout << "smash: process " << job.getPID() <<" was killed" << std::endl;
    kill(job.getPID(), SIGKILL);
  }
  removeFinishedJobs();
}

void JobsList::removeFinishedJobs() {
  for (auto it = jobs.begin(); it != jobs.end();) {
    int status = 0;
    pid_t result = waitpid(it->getPID(), &status, WNOHANG);
    if ((WIFSIGNALED(status)) || (result == it->getPID() && (WIFEXITED(status)))) {
      it = jobs.erase(it);
    }
    else {
      ++it;
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

pid_t JobsList::JobEntry::getPID()
{
    return this->pid;
}

string JobsList::JobEntry::getCommandLine()
{
    return this->command->getLine();
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
  return nullptr;
}

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {
  this->cmd_line = cmd_line;
}

void ExternalCommand::execute() {
  bool backgroundFlag = _isBackgroundComamnd(this->cmd_line.c_str());  // check if background
  char* ptr = (char *)this->cmd_line.c_str();
  if (backgroundFlag) {
    _removeBackgroundSign(ptr);
  }

  char* args[COMMAND_MAX_ARGS + 1];
  int numOfArgs = _parseCommandLine(ptr, args);
  if (!numOfArgs) return;
  
  args[COMMAND_MAX_ARGS] = nullptr;

  pid_t pid = fork();
  if (pid == -1) {
    perror("smash error: fork failed");
    return;
  }

  if (pid == 0) {  // Child process
    setpgrp();

    if(isInPATHEnvVar(args[0])) {           // check if in PATH environment
      if (containsWildcards(args)) {
        const char* bashPath = "/bin/bash";
        const char* const bashArgs[] = {bashPath, "-c", (char*)cmd_line.c_str(), 0};

        execv(bashPath, const_cast<char**>(bashArgs));
        perror("smash error: execv failed");
      }
      else {
        execvp(args[0], args);
        perror("smash error: execvp failed");
      }
      exit(EXIT_FAILURE);
    }
    else {
      exit(0);
    }
  }
  else if (!backgroundFlag){  // Parent process
    int status;
    if (waitpid(pid, &status, 0) == -1) {
      perror("smash error: waitpid failed");
    }
  }
  else {
    // bye felicia
    SmallShell::getInstance().getJobs()->addJob(this, pid);
  }

  for (int i = 0; i < numOfArgs; ++i) {
    free(args[i]);
  }
}

SmallShell::SmallShell() {
// TODO: add your implementation
  this->promptName = "smash> ";
  this->currPwd = getcwd(nullptr, 0);
  this->lastPwd = "-1";
  this->builtInCommands = {"alias", "unalias", "chprompt", "showpid", "pwd", "cd", "jobs", "fg", "quit", "kill"};
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
  char* args[COMMAND_MAX_ARGS];
  int argCount = _parseCommandLine(this->dir_to_repl.c_str(), args);
  SmallShell &smash = SmallShell::getInstance();
  string cwd = smash.getCurrentDirectory();

  // no dir, do nothing
  if (argCount == 0){
    _argsFree(argCount, args);
    return;
  }
  // max args allowed is 1:
  else if (argCount > 1){
    std::cerr << "smash error: cd: too many arguments" << std::endl;
  }
  // go back to previous dir
  else if (strcmp(args[0] ,"-") == 0){
    // if dir wasn't changed before
    if (strcmp(smash.lastPwd.c_str() ,"-1") == 0){
      std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
    }
    // change to prev dir
    else{
      if(chdir(smash.lastPwd.c_str()) != 0){
        std::perror("smash error: chdir failed");
      }
      else{
        smash.lastPwd = cwd;
        smash.setCurrentDirectory(getcwd(nullptr, 0));
      }
    }
  }
  // change to new dir and update prev dir to current
  else{
      if(chdir(args[0]) != 0){
        std::perror("smash error: chdir failed");
      }
      else{
        std::cerr << "the operation is " << args[0] << std::endl;
        smash.lastPwd = cwd;
        smash.setCurrentDirectory(getcwd(nullptr, 0));
      }
  }
  _argsFree(argCount, args);
  return;
}

/**
* aliasCommand
*/
aliasCommand::aliasCommand(const char *cmd_line, std::map<std::string, std::string>& aliasMap,  std::set<std::string>& builtInCommands): 
                                                                                                                                        aliasMap(aliasMap),
                                                                                                                                        builtInCommands(builtInCommands), 
                                                                                                                                        BuiltInCommand(cmd_line)
{
  this->newCommand = _getSecondArg(cmd_line);
}

void aliasCommand::execute(){
  // only alias was in cmd -> print of aliased commands
  if (strcmp(this->newCommand.c_str(), "") == 0){
    for (const auto& alias : aliasMap) {
      std::cout << alias.first << "='" << alias.second << "'" << std::endl;
    }
  }
  else{
    // find the name and the command
    size_t start = this->newCommand.find_first_of("'");
    size_t end = this->newCommand.find_last_not_of("'");
    std::string name,  command;
    if (start != std::string::npos && end != std::string::npos && start!=end){
      name = this->newCommand.substr(0, start-1); // end of name is (start-2)+1
      command = this->newCommand.substr(start, end+1);
      // alias name conflicts with an existing alias or a reserved keyword
      if(this->builtInCommands.find(name) != this->builtInCommands.end() || this->aliasMap.find(name) != this->aliasMap.end()){
        std::cerr << "smash error: alias: "<< name <<" already exists or is a reserved command" << std::endl;
      }
      // add to map the aliased name and command
      else{
        // validate the format and syntax
        if (std::regex_match(name, std::regex("^[a-zA-Z0-9_]+$"))){
          aliasMap[name] = command;
        }
        else{
          std::cerr << "smash error: alias: invalid syntax" << std::endl;
        }
      }
    }
    // invalid syntax
    else{
      std::cerr << "smash error: alias: invalid syntax" << std::endl;
    }
  }
}

/**
* unaliasCommand
*/

unaliasCommand::unaliasCommand(const char *cmd_line, std::map<std::string, std::string>& aliasMap): aliasMap(aliasMap), BuiltInCommand(cmd_line){
  this->cmd_line = _trim(string(cmd_line));
}

void unaliasCommand::execute(){
  // format the cmd-line
  char* args[COMMAND_MAX_ARGS];
  int argCount = _parseCommandLine(this->cmd_line.c_str(), args);

  if (argCount == 1){
    std::cerr << "smash error: unalias: not enough arguments" << std::endl;
  }
  for( int i = 1; i<argCount; i++){
    // the name found
    if(this->aliasMap.find(args[i]) != this->aliasMap.end()){
      this->aliasMap.erase(args[i]);
    }
    else{
      std::cerr << "smash error: unalias: "<< args[i] <<" alias does not exist" << std::endl;
      break;
    }
  }
  _argsFree(argCount, args);
}

// returns converted cmd in case of aliased command. if not aliased - does nothing.
const char* replaceAliased(const char *cmd_line, const map<std::string, std::string> map ){
  string firstWord = _getFirstArg(cmd_line);
  string cmd_s = _trim(string(cmd_line));
  size_t firstSpace = cmd_s.find(' ');

  if(map.find(firstWord) != map.end()){
    string aliasedCommand = map.at(firstWord);
    string args = (firstSpace == string::npos) ? "" : cmd_s.substr(firstSpace);
    return (aliasedCommand + args).c_str();
  }
  return cmd_line;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {

  char* args[COMMAND_MAX_ARGS];
  string line = cmd_line;     //for some reason doesnt want to parse a const smh
  int numOfArgs = _parseCommandLine(line.c_str(), args);
  if (!numOfArgs) return nullptr;
  bool redirectionCommand = isRedirection(numOfArgs, args);

  SmallShell &smash = SmallShell::getInstance();
  const char* cmd_s = replaceAliased(cmd_line, smash.aliasMap);

  string firstWord = args[0];                                           //it was already a string and its easy to use
  _argsFree(numOfArgs, args);                                     //free the memory of its sins

  if (redirectionCommand) {                                             // check to see if IO command first
    return new RedirectIOCommand(cmd_s);
  }
  else if (firstWord.compare("alias") == 0) {
    return new aliasCommand(cmd_s, smash.aliasMap, smash.builtInCommands);
  }
  else if (firstWord.compare("unalias") == 0) {
    return new unaliasCommand(cmd_s, smash.aliasMap);
  }
  else if (firstWord.compare("chprompt") == 0) { 
    return new CHPromptCommand(cmd_s);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_s);
  }
  else if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_s);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_s, nullptr);
  }
  else if (firstWord.compare("jobs") == 0) {
    return new JobsCommand(cmd_s, jobs);
  }
  else if (firstWord.compare("fg") == 0) {
    return new FGCommand(cmd_s);
  }
  else if (firstWord.compare("quit") == 0) {
    return new QuitCommand(cmd_s, this->jobs);
  }
  else if (firstWord.compare("kill") == 0) {
    return new KillCommand(cmd_s, this->jobs);
  }
  else if (firstWord.compare("whoami") == 0) {
    return new WhoAmICommand(cmd_s);
  }
  else {
    return new ExternalCommand(cmd_s);
  }

    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  Command* cmd = CreateCommand(cmd_line);
  if(cmd) cmd->execute();
}

void SmallShell::setCurrentDirectory(const std::string newDir){
  this->currPwd = newDir;
}

WhoAmICommand::WhoAmICommand(const char *cmd_line) : Command(cmd_line){
  this->cmd_line = cmd_line;
}
void WhoAmICommand::execute() {
  char *userEnv = getenv("USER");                       //contains username
  if (!userEnv) {
    perror("smash error: getenv failed");
    return;
  }
  char *homeEnv = getenv("HOME");                       //contains home directory of usename
  if (!homeEnv) {
    perror("smash error: getenv failed");
    return;
  }
  std::cout << userEnv << " " << homeEnv << std::endl;
}

RedirectIOCommand::RedirectIOCommand(const char *cmd_line) : Command(cmd_line){
  this->cmd_line = cmd_line;
}

void RedirectIOCommand::execute() {

  int i, fileFD;
  char* args[COMMAND_MAX_ARGS + 1];
  int numOfArgs = _parseCommandLine(this->cmd_line.c_str(), args);    // parse the command line
  if (!numOfArgs) return;

  for (i = 0; i < numOfArgs; i++) {                                   // find the index that contains the redirection symbol
    if (!strcmp(args[i], ">") || !strcmp(args[i], ">>")) {
      break;
    }
  }

  string arrow = args[i], firstArg, thirdArg;

  if (!arrow.compare(">>")) {                                         // open with seek pointer in the end
    firstArg = _trim(this->cmd_line.substr(0, this->cmd_line.find_first_of(">>")));
    thirdArg = _trim(this->cmd_line.substr(this->cmd_line.find_first_of(">>") + 2));   
    fileFD = open(thirdArg.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
  }
  else {                                                              // open with seek pointer at the start
    firstArg = _trim(this->cmd_line.substr(0, this->cmd_line.find_first_of(">")));
    thirdArg = _trim(this->cmd_line.substr(this->cmd_line.find_first_of(">") + 2));   
    fileFD = open(thirdArg.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  }

  _argsFree(numOfArgs, args);                                   // release that bitch

  if (fileFD == -1) { // if no file me sad
    perror("smash error: open failed");
    return;
  }

  int oldSTDOUT = dup(STDOUT_FILENO);                             // duplicate the stdout file descriptor
  if (oldSTDOUT == -1) { //if no stdout then how print before
    perror("smash error: dup failed");
    close(fileFD);
    exit(EXIT_FAILURE);
  }

  if (dup2(fileFD, STDOUT_FILENO) == -1) {                        //change output redirection to the file
    perror("smash error: dup2 failed");
    close(fileFD);
    exit(EXIT_FAILURE);
  }

  SmallShell::getInstance().executeCommand(firstArg.c_str());    //execute the command in the first argument 

  if (dup2(oldSTDOUT, STDOUT_FILENO) == -1) {                    //change output redirection to stdout
    perror("smash error: dup2 failed");
    close(fileFD);
    exit(EXIT_FAILURE);
  }

  if (close(fileFD) == -1) {                                      //close file descriptors
    perror("smash error: close failed");
  }
    if (close(oldSTDOUT) == -1) {
    perror("smash error: close failed");
  }

}