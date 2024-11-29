#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
    // TODO: Add your data members
private:
    std::string cmd_line;
public:
    Command(const char *cmd_line);

    virtual ~Command(){
    }

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed

    std::string getLine();
};

class BuiltInCommand : public Command {
private:
    std::string cmd_line;
public:
    BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand() {
    }
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
private:
    std::string dir_to_repl;
    std::string* last_pwd;
    
public:
    // TODO: Add your data members public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd);

    virtual ~ChangeDirCommand() {
    }

    void execute() override;
};



class CHPromptCommand : public BuiltInCommand {
private:
    std::string newPromptName;
public:
    CHPromptCommand(const char *cmd_line);

    virtual ~CHPromptCommand() {
    }

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
private:
    int pid;
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() {
    }

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
private:
    std::string cmd_line;
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
private:
    // TODO: Add your data members public:
    std::string cmd_line;
    JobsList *jobs;
public:
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {
    }

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
    std::string cmd_line;
    JobsList *jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand() {
    }

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
    private:
        // TODO: Add your data members
        int ID;
        std::string name;
        std::string fullCommandArgs;
        
    public:
        bool isActive;
        JobEntry(int ID, const char* name, const char* fullCommandArgs, bool isStopped);

        ~JobEntry(){

        }

        void printEntry() const;

        bool equalsToID(int jobId);

        int getID();

        std::string getName();

        std::string getFullCommandArgs();
    };
    // TODO: Add your data members
    std::vector<JobEntry> jobs;
    int nextJobID;
public:
    JobsList();

    ~JobsList() {

    }

    void addJob(Command *cmd, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);

    int zombieTheJob(int jobId);

    // TODO: Add extra methods or modify exisitng ones as needed
    int getLastJobID();

    bool isEmpty();
};

class JobsCommand : public BuiltInCommand {
    // TODO: Add your data members
    std::string cmd_line;
    JobsList *jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs);

    virtual ~JobsCommand() {
    }

    void execute() override;
};

class FGCommand : public BuiltInCommand {
    // TODO: Add your data members
    std::string cmd_line;
public:
    FGCommand(const char *cmd_line);

    virtual ~FGCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class ListDirCommand : public Command {
public:
    ListDirCommand(const char *cmd_line);

    virtual ~ListDirCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class NetInfo : public Command {
    // TODO: Add your data members
public:
    NetInfo(const char *cmd_line);

    virtual ~NetInfo() {
    }

    void execute() override;
};

class aliasCommand : public BuiltInCommand {
public:
    aliasCommand(const char *cmd_line);

    virtual ~aliasCommand() {
    }

    void execute() override;
};

class unaliasCommand : public BuiltInCommand {
public:
    unaliasCommand(const char *cmd_line);

    virtual ~unaliasCommand() {
    }

    void execute() override;
};


class SmallShell {
private:
    // TODO: Add your data members
    std::string promptName;
    std::string currPwd;
    std::string lastPwd;
    JobsList* jobs;

    SmallShell();

public:
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);

    // TODO: add extra methods as needed

    void changePrompt(const std::string newPrompt);
    
    std::string getPrompt();

    std::string getCurrentDirectory();

    // todo , eva
    void setCurrentDirectory(const std::string newDir);

    std::string* getLastDirectory(){
        return &lastPwd;
    }

    JobsList::JobEntry* head();

    JobsList::JobEntry* tail();

    JobsList* getJobs();
};

#endif //SMASH_COMMAND_H_
