    // Ver: 04-11-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <list>
#include <map>
#include <set>

#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/syscall.h>

using namespace std;

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)


class Command {
    // TODO: Add your data members
protected:
    const char* cmd_line; // original command provided by the user(including spaces, and aliases )
    string cmd_line_str;
    string  alias_cmd;
    vector<string> args;
    int argc;
    bool background;
public:
    Command(const char *cmd_line);

    virtual ~Command(){};

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
    const char* get_cmd_line() {
        return cmd_line;
    }
    //const string& arg(int i) const { return args_.at(i); } // throws if invalid
    //const vector<string>& args() const { return args_; }
    void setAliasCmd(string alias_cmd) {
        this->alias_cmd = alias_cmd;
    }
    string getAliasCmd() const {
        return alias_cmd;
    }
};

class JobEntry {//I changed JobEntry to struct
public:
    bool isStopped;
    string cmd;
    int id;
    pid_t pid;
    string aliasCmd; // If its not alias aliasCmd=""
    JobEntry(string cmd, int id, pid_t pid, string aliasCmd, bool isStopped=false)
    : isStopped(isStopped), cmd(cmd), id(id), pid(pid), aliasCmd(aliasCmd) {}
    // TODO: Add your data members

    string getOriginalCmdLine() {
        if(aliasCmd!="")
            return aliasCmd;
        return cmd;
    }
};

class JobsList {
private: // changed from public to private and moved JobEntry outside
    list<JobEntry*> jobs;
    // TODO: Add your data members
public:
    JobsList(){};

    ~JobsList() {
        for(JobEntry* job : jobs) delete job;
        jobs.clear();
    }

    void addJob(string cmd, pid_t pid, string aliasCmd, bool isStopped = false) {
        removeFinishedJobs();
        int id =1;
        for(JobEntry* job : jobs)
            id = max(job->id+1, id);

        JobEntry *job = new JobEntry(cmd,id,pid,aliasCmd, isStopped);
        jobs.push_back(job);

    }

    void printJobsList() {
        for(JobEntry* job : jobs) {
            cout<<"["<<job->id<<"] "<<job->getOriginalCmdLine()<<endl;
        }
    }

    void printForKill() {
        cout<<"smash: sending SIGKILL signal to "<<jobs.size()<<" jobs:"<<endl;
        for(JobEntry* job : jobs) {
            cout<<job->pid<<": "<<job->getOriginalCmdLine()<<endl;
        }
    }

    void killAllJobs() {
        for(JobEntry* job : jobs) {
            kill(job->pid,SIGKILL);
            delete job;
        }
        jobs.clear();
    }

    void removeFinishedJobs() {
        auto it = jobs.begin();
        while(it!=jobs.end()){
            JobEntry *job = *it;
            int status = 0;
            pid_t ret = waitpid(job->pid, &status, WNOHANG);
            if(ret == -1) {
                perror("smash error: waitpid failed");
                ++it;
            }
            else if(ret == 0) {
                ++it;
            }
            else {
                delete job;
                it = jobs.erase(it);
            }
        }
    }

    JobEntry *getJobById(int jobId) {
        for(JobEntry* job : jobs) {
            if(jobId ==job->id)
                return job;
        }
        return nullptr;
    }

    void removeJobById(int jobId) {
        for(auto it = jobs.begin(); it != jobs.end(); ++it) {
            if(jobId ==(*it)->id) {
                delete (*it);
                jobs.erase(it);
                return;
            }
        }

    }

    JobEntry *getLastJob(int *lastJobId) {
        if(jobs.empty())
            return nullptr;
        *lastJobId = jobs.back()->id;
        return jobs.back();
    }

    //JobEntry *getLastStoppedJob(int *jobId);

    bool empty() const {
        return jobs.empty();
    }
    // TODO: Add extra methods or modify exisitng ones as needed
};

class SmallShell {
private:
    // TODO: Add your data members
    string prompt;
    int pid;
    string prevPwd;
    JobsList* jobsList;
    vector<pair<string,string>> aliasPairs;
    map<string,string> aliasMap;
    int foreground_pid;
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

    // TODO: add extra methods as needed

    void setPrompt(const string& prompt){
        this->prompt = prompt;
    }
    string getPrompt() const{
        return prompt;
    }
    void executeCommand(const char *cmd_line,string alias_cmd);
    int getPid() const{
        return pid;
    }
    void setForegroundPid(int current_pid) {
        foreground_pid = current_pid;
    }
    string getpwd() const{
        char buff[COMMAND_MAX_LENGTH];
        getcwd(buff, COMMAND_MAX_LENGTH);
        return string(buff);
    }

    string getPrevPwd() const {
        return prevPwd;
    }
    void setPrevPwd(string prevPwd)  {
        this->prevPwd = prevPwd;
    }

    JobsList* getJobsList() const {
        return jobsList;
    }

    vector<pair<string,string>>& getAliasPairs() {
        return aliasPairs;
    }
    map<string,string>& getAliasMap() {
        return aliasMap;
    }

    string getAlias(const string& alias){
        auto it = aliasMap.find(alias);
        if (it != aliasMap.end()) return it->second;
        return string();
    }
    bool validCommand(string cmd) {
        static const set<std::string> reserved = {
            "chprompt","showpid","pwd","cd","jobs","fg","quit","kill",
            "alias","unalias","unsetenv","sysinfo","du", "whoami", "usbinfo"
            // add any other internal commands
        };
        return reserved.find(cmd) != reserved.end();
    }

    void ctrlCHandler() {
        if(foreground_pid==-1)
            return;

        kill(foreground_pid,SIGINT);
        cout<<"smash: process "<<foreground_pid<<" was killed"<<endl;
        foreground_pid=-1;
    }

};


class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line):Command(cmd_line) {};

    virtual ~BuiltInCommand() {
    }
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line): Command(cmd_line){}

    virtual ~ExternalCommand() {
    }

    void execute() override;

};


class RedirectionCommand : public Command {
    // TODO: Add your data members
    string line;
public:
    RedirectionCommand(const char *cmd_line,string cmd_s):Command(cmd_line),line(cmd_s){}

    virtual ~RedirectionCommand() {
    }

    void execute() override;


};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line):Command(cmd_line){}

    virtual ~PipeCommand() {

    }

    void execute() override;
};


class DiskUsageCommand : public Command {
    // define linux_dirent64 if not provided by headers
    struct linux_dirent64 {
        ino64_t        d_ino;
        off64_t        d_off;
        unsigned short d_reclen;
        unsigned char  d_type;
        char           d_name[4096]; // flexible array
    };

    long long compute_disk_usage_blocks(string path);
    long long compute_disk_usage_kb(string path);
public:
    DiskUsageCommand(const char *cmd_line):Command(cmd_line){};

    virtual ~DiskUsageCommand() {
    }

    void execute() override;

};




class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line):Command(cmd_line){}

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};



class USBInfoCommand : public Command {
    // TODO: Add your data members **BONUS: 10 Points**
    struct linux_dirent64 {
        ino64_t        d_ino;
        off64_t        d_off;
        unsigned short d_reclen;
        unsigned char  d_type;
        char           d_name[4096]; // flexible array
    };
public:
    USBInfoCommand(const char *cmd_line):Command(cmd_line){}

    virtual ~USBInfoCommand() {
    }
    string read_sysfs_file(const string& dir, const string& name) {
        string path = dir + "/" + name;
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) return "";
        const size_t BUF_SZ = 4096;
        char buf[BUF_SZ+1];
        ssize_t r = read(fd, buf, BUF_SZ);
        close(fd);
        if(r<=0) return "";
        buf[r]='\0';
        string s(buf);
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    void execute() override;

    vector<string> list_dir_names(const char *path);
};

class ChangeDirCommand : public BuiltInCommand {
    // TODO: Add your data members public:
public:
    ChangeDirCommand(const char *cmd_line):BuiltInCommand(cmd_line){}// I removed char **plastPwd

    virtual ~ChangeDirCommand() {
    }

    void execute() override;

};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line):BuiltInCommand(cmd_line){}

    virtual ~GetCurrDirCommand() {
    }

    void execute() override{
        cout<<SmallShell::getInstance().getpwd()<<endl;
    }
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line): BuiltInCommand(cmd_line) {};

    virtual ~ShowPidCommand() {
    }

    void execute() override{
         cout<<"smash pid is "<<SmallShell::getInstance().getPid()<<endl;
    }
};


class ChangePromptCommand: public BuiltInCommand {
public:
    ChangePromptCommand(const char *cmd_line):BuiltInCommand(cmd_line) {};

    virtual ~ChangePromptCommand() {
    }

    void execute() override;

};


class QuitCommand : public BuiltInCommand {
    // TODO: Add your data members
    JobsList *jobs;
public:
    QuitCommand(const char *cmd_line, JobsList *jobs):BuiltInCommand(cmd_line),jobs(jobs){}

    virtual ~QuitCommand() {
    }

    void execute() override {
        if(argc>=2 && args[1]=="kill") {
            jobs->removeFinishedJobs();
            jobs->printForKill();
            jobs->killAllJobs();
        }
        exit(0);
    }
};


class JobsCommand : public BuiltInCommand {
    // TODO: Add your data members
    JobsList *jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line),jobs(jobs){}

    virtual ~JobsCommand() {
    }

    void execute() override {
        jobs->removeFinishedJobs();
        jobs->printJobsList();
    }
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
    JobsList *jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs):BuiltInCommand(cmd_line),jobs(jobs){}

    virtual ~KillCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
    JobsList *jobs;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs):BuiltInCommand(cmd_line),jobs(jobs){}

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    AliasCommand(const char *cmd_line):BuiltInCommand(cmd_line){}

    virtual ~AliasCommand() {
    }

    void execute() override;
};

class AliasDec: public Command {
    string alias;
public:
    AliasDec(const char* cmd_line, string alias): Command(cmd_line), alias(alias){}
    virtual ~AliasDec() {}
    void execute() override;

};


class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line): BuiltInCommand(cmd_line){};

    virtual ~UnAliasCommand() {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line): BuiltInCommand(cmd_line){};

    virtual ~UnSetEnvCommand() {
    }


    void execute() override;


};

class SysInfoCommand : public BuiltInCommand {
public:
    SysInfoCommand(const char *cmd_line): BuiltInCommand(cmd_line){};

    virtual ~SysInfoCommand() {
    }

    ssize_t safe_read_all(int fd, char *buf, size_t bufsize);

    long long parse_positive_decimal(const char *s);

    void execute() override;
};



#endif //SMASH_COMMAND  _H_
