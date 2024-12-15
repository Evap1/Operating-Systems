#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <set>
#include <map>
#include <list>
#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define INVALID_PDIR ("-1")
#define INVALID_DIR ("-1")
#define INVALID ("-1")
#define BUF (4096)
#define DEFAULT_GATEWAY_DEST "00000000"
#define INITIAL_FG (-1)
#define BUF_SIZE (1024)
/**--------------------------------------------------------------------*/

struct linux_dirent {
    unsigned long d_ino;     
    unsigned long d_off;     
    unsigned short d_reclen; // length of this entry
    char d_name[];           // start of the name (null-terminated)
};

/**--------------------------------------------------------------------*/

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

/**--------------------------------------------------------------------*/

class JobsList; // used in a built in command, declared priorly

/**--------------------------------------------------------------------*/

class BuiltInCommand : public Command {
private:
    std::string cmd_line;
public:
    BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand() {
    }
};

/**--------------------------------------------------------------------*/

class ExternalCommand : public Command {
private:
    std::string cmd_line;
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

class PipeCommand : public Command {
private:
    std::string cmd_line;
    std::tuple<std::string, size_t> position;
public:
    PipeCommand(const char *cmd_line, std::tuple<std::string, size_t> pos);

    virtual ~PipeCommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

class RedirectIOCommand : public Command {
private:
    // TODO: Add your data members
    std::string cmd_line;
    std::tuple<std::string, size_t> position;
public:
    explicit RedirectIOCommand(const char *cmd_line, std::tuple<std::string, size_t> pos);

    virtual ~RedirectIOCommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

class ListDirCommand : public Command {
private:
    std::string target_dir;

    void listDirectory(int dir_fd, const std::string& path, int indent_level);

public:
    ListDirCommand(const char *cmd_line);

    virtual ~ListDirCommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

class WhoAmICommand : public Command {
private:
    std::string cmd_line;
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

class NetInfo : public Command {
private:
    std::string cmd_line;

    bool isInterfaceValid(const std::string &interface);

    void printIPSubnet(const std::string &interface);

    void printDeafultGateway(const std::string &interface);

    void printDNSServers();

public:
    NetInfo(const char *cmd_line);

    virtual ~NetInfo() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/
/**---------------------------BuiltInCommands--------------------------*/
/**--------------------------------------------------------------------*/

class ChangeDirCommand : public BuiltInCommand {
private:
    std::string cmd_line;
    std::string last_pwd;
    
public:
    // TODO: Add your data members public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd);

    virtual ~ChangeDirCommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

class CHPromptCommand : public BuiltInCommand {
private:
    std::string newPromptName;
public:
    CHPromptCommand(const char *cmd_line);

    virtual ~CHPromptCommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

class ShowPidCommand : public BuiltInCommand {
private:
    int pid;
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

class GetCurrDirCommand : public BuiltInCommand {
private:
    std::string cmd_line;
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

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

/**--------------------------------------------------------------------*/

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

/**--------------------------------------------------------------------*/

class FGCommand : public BuiltInCommand {
    // TODO: Add your data members
    std::string cmd_line;
public:
    FGCommand(const char *cmd_line);

    virtual ~FGCommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

class aliasCommand : public BuiltInCommand {
private:
    std::string cmd_line;
    std::map<std::string, std::string>& alias_map;
    std::set<std::string>& built_in_commands;
    std::list<std::pair<std::string, std::string>>& alias_list;

public:
    aliasCommand(const char *cmd_line, std::map<std::string, std::string>& alias_map, std::set<std::string>& built_in_commands, std::list<std::pair<std::string, std::string>>& alias_list);

    virtual ~aliasCommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

class unaliasCommand : public BuiltInCommand {
private:
    std::string cmd_line;
    std::map<std::string, std::string>& alias_map;
    std::list<std::pair<std::string, std::string>>& alias_list;
public:
    unaliasCommand(const char *cmd_line, std::map<std::string, std::string>& alias_map, std::list<std::pair<std::string, std::string>>& alias_list);

    virtual ~unaliasCommand() {
    }

    void execute() override;
};

/**--------------------------------------------------------------------*/

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

/**--------------------------------------------------------------------*/
/**---------------------------JobsListClass----------------------------*/
/**--------------------------------------------------------------------*/

class JobsList {
public:
    class JobEntry {
    private:
        // TODO: Add your data members
        int ID;
        pid_t pid;
        Command* command;
        
    public:
        bool isActive;
        JobEntry(int ID, pid_t pid, Command* command, bool isActive);

        ~JobEntry(){

        }

        void printEntry() const;

        bool equalsToID(int jobId);

        int getID();

        pid_t getPID();

        std::string getCommandLine();
    };
    // TODO: Add your data members
    std::vector<JobEntry> jobs;
    int nextJobID;
public:
    JobsList();

    ~JobsList() {

    }

    void addJob(Command *cmd, pid_t pid, bool isActive = true);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    // TODO: Add extra methods or modify exisitng ones as needed
    int getLastJobID();

    bool isEmpty();
};

/**--------------------------------------------------------------------*/
/**---------------------------SmallShell-------------------------------*/
/**--------------------------------------------------------------------*/

class SmallShell {
private:
    // TODO: Add your data members
    std::string promptName;
    std::string currPwd;
    JobsList* jobs;
    pid_t fg_pid;

    SmallShell();

public:
    std::set<std::string> built_in_commands;
    std::set<std::string> built_in_commands_bg;
    std::map<std::string, std::string> alias_map;
    std::list<std::pair<std::string, std::string>> alias_list;
    std::string lastPwd;
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

    void setCurrentDirectory(const std::string newDir);

    JobsList::JobEntry* head();

    JobsList::JobEntry* tail();

    JobsList* getJobs();

    void setFGPID(pid_t pid);

    pid_t getFGPID();
};

#endif //SMASH_COMMAND_H_
