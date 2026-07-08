#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <sys/wait.h>
#include <stdio.h>
#include <regex>
#include <ctime>
#include "Commands.h"


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
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)));
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memcpy(args[i], s.c_str(), s.length() + 1);
        ++i;
        if (i >= COMMAND_MAX_ARGS - 1) break; // guard
    }
    args[i] = NULL;
    return i;
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line ? cmd_line : "");
    size_t idx = str.find_last_not_of(WHITESPACE);
    if (idx == string::npos) return false;
    return str[idx] == '&';
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

// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell():prompt("smash"), pid(getpid()), prevPwd(""), jobsList(new JobsList),foreground_pid(-1) {
    // TODO: add your implementation
}

SmallShell::~SmallShell() {
    delete jobsList;
    // TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    if (cmd_s.empty()) {
        return nullptr;   // nothing to do for empty command line
    }
    _removeBackgroundSign(&cmd_s[0]);
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if(firstWord.compare("alias")==0) {
        regex expression("^alias [a-zA-Z0-9_]+='[^']*'$");
        if (regex_match(cmd_s, expression)) {
            return new AliasCommand(cmd_line);
        } else {
            // invalid alias format: if it contains redirection or pipe, treat those accordingly
            if (cmd_s.find(">>") != string::npos || cmd_s.find('>') != string::npos)
                return new RedirectionCommand(cmd_line, cmd_s);
            if (cmd_s.find("|&") != string::npos || cmd_s.find('|') != string::npos)
                return new PipeCommand(cmd_line);
            // otherwise keep alias behavior (will print invalid format error in AliasCommand::execute)
            return new AliasCommand(cmd_line);
        }
    }
    else if(cmd_s.find('>') != string::npos || cmd_s.find(">>") != string::npos)
        return new RedirectionCommand(cmd_line,cmd_s);
    else if(cmd_s.find('|') != string::npos || cmd_s.find("|&") != string::npos)
        return new PipeCommand(cmd_line);
    else if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    }
    else if (firstWord.compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }
    else if(firstWord.compare("chprompt") == 0)
        return new ChangePromptCommand(cmd_line);
    else if(firstWord.compare("cd") == 0)
        return new ChangeDirCommand(cmd_line);
    else if(firstWord.compare("jobs") == 0)
        return new JobsCommand(cmd_line,jobsList);
    else if(firstWord.compare("fg") == 0)
        return new ForegroundCommand(cmd_line,jobsList);
    else if(firstWord.compare("quit")==0)
        return new QuitCommand(cmd_line,jobsList);
    else if(firstWord.compare("kill")==0)
        return new KillCommand(cmd_line,jobsList);
    else if(firstWord.compare("unalias")==0)
        return new UnAliasCommand(cmd_line);
    else if(firstWord.compare("unsetenv")==0)
        return new UnSetEnvCommand(cmd_line);
    else if(firstWord.compare("sysinfo")==0)
        return new SysInfoCommand(cmd_line);
    else if(firstWord.compare("whoami")==0)
        return new WhoAmICommand(cmd_line);
    else if(firstWord.compare("du")==0)
        return new DiskUsageCommand(cmd_line);
    else if(firstWord.compare("usbinfo")==0) {
        return new USBInfoCommand(cmd_line);
    }
    else if(aliasMap.find(firstWord)!=aliasMap.end() && !aliasMap.find(firstWord)->second.empty())
        return new AliasDec(cmd_line, firstWord);
    else
        return new ExternalCommand(cmd_line);


    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line,string alias_cmd) {
    Command* cmd = CreateCommand(cmd_line);
    if (!cmd) {
        return;
    }
    cmd->setAliasCmd(alias_cmd);
    cmd->execute();
    delete cmd;
}


Command::Command(const char *cmd_line):cmd_line(cmd_line),cmd_line_str(cmd_line ? cmd_line : ""), args()  {
    // own a copy of the original command line
    if (!cmd_line_str.empty()) {
        char* cmd_buf = &cmd_line_str[0];

        background = _isBackgroundComamnd(cmd_buf );
        if (background) {
            _removeBackgroundSign(cmd_buf );
        }

        char* c_args[COMMAND_MAX_ARGS] = { nullptr };
        int n = _parseCommandLine(cmd_buf, c_args);

        for (int i = 0; i < n; ++i) {
            if (c_args[i]) {
                args.emplace_back(c_args[i]); // copy into std::string
                free(c_args[i]);              // _parseCommandLine used malloc
                c_args[i] = nullptr;
            }
        }
        argc = static_cast<int>(args.size());
    }
    else {
        argc=0;
        background=false;
    }



}

//ChangePromptCommand
void ChangePromptCommand::execute(){
    string newPrompt="smash";
    SmallShell &sh = SmallShell::getInstance();
    if(argc>1) {
        string prompt = args[1];

        //maybe need to remove & sign if not do so in command constructor
        if(!prompt.empty())
            newPrompt = prompt;
    }

    sh.setPrompt(newPrompt);
}


//ChangeDirCommand
void ChangeDirCommand::execute(){
    if(argc==1)
        return;
    if(argc>2) {
        cerr<<"smash error: cd: too many arguments"<<endl;
        return;
    }
    string oldPwd = SmallShell::getInstance().getpwd();
    string path = args[1];
    string newPwd = path;
    if(path=="-") {
        string prev = SmallShell::getInstance().getPrevPwd();
        if(prev.empty()) {
            cerr<<"smash error: cd: OLDPWD not set"<<endl;
            return;
        }
        newPwd = prev;
    }
    if(chdir(newPwd.c_str())<0){
        perror("smash error: chdir failed");
        return;
    }
    SmallShell::getInstance().setPrevPwd(oldPwd);
}

//ForegroundCommand
void ForegroundCommand::execute(){
    jobs->removeFinishedJobs();
    if (!jobs) {
        cerr<<"smash error: fg: jobs list is empty"<<endl;
        return;
    }
    JobEntry* job;
    int id;
    if(argc==1) {
        if(jobs->empty()) {
            cerr<<"smash error: fg: jobs list is empty"<<endl;
            return;
        }
        job = jobs->getLastJob(&id);
    }
    else if(argc==2) {
        try {
            id = stoi(args[1]);
        }catch(const std::exception& e) {//if args[1] is not a number
            cerr << "smash error: fg: invalid arguments" << std::endl;
            return;
        }
        if(id<0){
            cerr << "smash error: fg: invalid arguments" << std::endl;
            return;
        }

        job = jobs->getJobById(id);
        if(job==nullptr){
            cerr<<"smash error: fg: job-id "<<id<<" does not exist"<<endl;
            return;
        }
    }
    else {
        cerr<<"smash error: fg: invalid arguments"<<endl;
        return;
    }

    //string commandLine = job->cmd;
    cout<<job->getOriginalCmdLine()<<" "<<job->pid<<endl;

    SmallShell::getInstance().setForegroundPid(job->pid);

    if(job->isStopped)
        kill(job->pid, SIGCONT);

    jobs->removeJobById(job->id);
    waitpid(job->pid, nullptr,0);
    SmallShell::getInstance().setForegroundPid(-1);


}


//Killcommand
void KillCommand::execute() {
    if(argc!=3 || args[1].empty() || args[1][0]!='-') {
        cerr<<"smash error: kill: invalid arguments"<<endl;
        return;
    }
    jobs->removeFinishedJobs();
    int signum,id;

    string signStr = args[1].substr(1);
    if (signStr.empty()) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    try {
        signum = stoi(signStr);
        id = stoi(args[2]);
    } catch(const std::exception& e){
        cerr<<"smash error: kill: invalid arguments"<<endl;
        return;
    }
    if(id<0){
        cerr<<"smash error: kill: invalid arguments"<<endl;
        return;
    }
    JobEntry* job = jobs->getJobById(id);
    if(!job){
        cerr<<"smash error: kill: job-id "<<id<<" does not exist"<<endl;
        return;
    }

    cout<<"signal number "<<signum<<" was sent to pid "<<job->pid<<endl;
    if(kill(job->pid,signum) == -1) {
        perror("smash error: kill failed");
    }
}

void AliasDec::execute() {
    SmallShell& sh = SmallShell::getInstance();
    string firstPartCmd = sh.getAlias(alias);
    string line = cmd_line;
    string secondPartCmd = line.substr(alias.size());
    string command = (firstPartCmd + secondPartCmd);
    sh.executeCommand(command.c_str(),line);

}

//Alias
void AliasCommand::execute() {
    SmallShell& sh = SmallShell::getInstance();
    if(argc==1) {
        for(const pair<string,string>& p: sh.getAliasPairs()) {
            cout<<p.first<<"='"<<p.second<<"'"<<endl;
        }
        return;
    }


    string fullstr = _trim(this->cmd_line_str);


    // should handle the  background flag (trailing '&') handle it here if needed:
    // if (!full.empty() && backRound) { /* strip trailing & or space */ }

    regex expression("^alias [a-zA-Z0-9_]+='[^']*'$");
    if (!regex_match(fullstr, expression)) {
        cerr << "smash error: alias: invalid alias format" << endl;
        return;
    }

    string arg2 = fullstr.substr(args[0].size()+1); //Remove "alias " prefix (6 chars)

    string::size_type eq = arg2.find('=');
    if(eq==string::npos || eq==0) {
        cerr << "smash error: alias: invalid alias format" << endl;
        return;
    }
    string abbr = arg2.substr(0,eq);
    //after = '' is expected
    if(eq+1>=arg2.size()|| arg2[eq+1]!='\''){
        cerr << "smash error: alias: invalid alias format" << endl;
        return;
    }

    string::size_type start_quote = eq+1;
    string::size_type end_quete = arg2.find('\'', start_quote+1);
    if(end_quete==string::npos){
        cerr << "smash error: alias: invalid alias format" << endl;
        return;
    }

    string command = arg2.substr(start_quote+1, end_quete-(start_quote+1));

    map<string,string>& aliasMap = sh.getAliasMap();
    vector<pair<string,string>>& aliasP = sh.getAliasPairs();
    if(aliasMap.find(abbr)==aliasMap.end() && !sh.validCommand(abbr)) {
        aliasMap[abbr] = command;
        aliasP.emplace_back(abbr,command);
    }
    else
        cerr<< "smash error: alias: "<< abbr <<" already exists or is a reserved command"<<endl;

}


//Unalias
void UnAliasCommand::execute() {
    SmallShell& sh = SmallShell::getInstance();
    if(argc==1) {
        cerr<<"smash error: unalias: not enough arguments"<<endl;
        return;
    }
    map<string,string>& aliasMap = sh.getAliasMap();
    vector<pair<string,string>>& aliasP = sh.getAliasPairs();

    for(int i = 1; i < argc; i++) {
        string ali = args[i];
        auto itMap = aliasMap.find(ali);
        if(itMap==aliasMap.end()){
            cerr<<"smash error: unalias: "<< ali << " alias does not exist"<<endl;
            return;
        }

        aliasMap.erase(itMap);

        for(auto it = aliasP.begin(); it != aliasP.end(); ++it) {
            if(it->first==ali){
                aliasP.erase(it);
                break;
            }
        }
    }
}

//helper function for externalcommand execute
static bool complexExternal(const char* cmd_line){
    string line = cmd_line;
    return line.find('*')!=string::npos || line.find('?')!=string::npos;
}

void ExternalCommand::execute(){
    if(argc==0)
        return;

    int pid = fork();
    if(pid<0) {
        perror("smash error: fork failed");
        return;
    }
    if(pid == 0) {
        if(setpgrp()==-1) {
            perror("smash error: setpgrp failed");
            exit(1);
        }

        if(complexExternal(cmd_line)) {
            const char*  bash_args[4] = {"/bin/bash", "-c", cmd_line, nullptr};
            execv("/bin/bash", (char* const*)bash_args);
            perror("smash error: execv failed");
            exit(1);
        }

        //vector<char*> argv(COMMAND_MAX_ARGS);
        std::vector<char*> argv(argc + 1, nullptr);
        for (int i = 0; i < argc; ++i) {
            // points into the std::string storage (valid while this object lives)
            argv[i] = const_cast<char*>(args[i].c_str());
        }

        argv[argc] = nullptr;
        execvp(argv[0],  argv.data());
        perror("smash error: execvp failed");
        exit(1);
    }
    else {
        if(!background) {
            int status = 0;
            SmallShell::getInstance().setForegroundPid(pid);
            if(waitpid(pid,&status,0)==-1)
                perror("smash error: waitpid failed");
            SmallShell::getInstance().setForegroundPid(-1);
        }
        else {
            JobsList* jobs=SmallShell::getInstance().getJobsList();
            jobs->addJob(this->get_cmd_line(),pid, this->getAliasCmd());
        }
    }
}

void RedirectionCommand::execute() {
    SmallShell& sh = SmallShell::getInstance();

    size_t pos1 = line.find('>');
    size_t pos2 = line.find(">>");
    string command;
    int fd;


    //string path = args[argc-1];
    string path;
    if (pos2 != string::npos) {
        command = line.substr(0, pos2);
        path = _trim(line.substr(pos2 + 2)); // everything after ">>"
    } else if (pos1 != string::npos) {
        command = line.substr(0, pos1);
        path = _trim(line.substr(pos1 + 1)); // everything after ">"
    }
    size_t ws = path.find_first_of(" \t");
    if (ws != string::npos) path = path.substr(0, ws);

    if(pos2!=line.npos) {
        command = line.substr(0,pos2);
        fd = open(path.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0666);
        if(fd<0) {
            perror("smash error: open failed");
            return;
        }
    }
    else if(pos1!=line.npos) {
        //case >
        command = line.substr(0,pos1);
        fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(fd<0) {
            perror("smash error: open failed");
            return;
        }
    }
    else
        return;
    command =  _trim(command);

    int savedStdout = dup(STDOUT_FILENO);
    if (savedStdout < 0) {
        perror("smash error: dup failed");
        close(fd);
        return;
    }

    if (dup2(fd, STDOUT_FILENO) < 0) {
        perror("smash error: dup2 failed");
        close(fd);
        close(savedStdout);
        return;
    }
    close(fd);

    sh.executeCommand(command.c_str(),"");

    if (dup2(savedStdout, STDOUT_FILENO) < 0) {
        perror("smash error: dup2 failed");
        // continue, but keep savedStdout open if dup2 failed
    }

    close(savedStdout);

}



void PipeCommand::execute() {
    //dup(old,new)
    SmallShell& sh = SmallShell::getInstance();
    string line = cmd_line;
    size_t idx1 = line.find("|&");
    size_t idx2 = line.find("|");
    if(idx1 != string::npos) {
        int fd[2];
        if(pipe(fd)==-1){ perror("smash error: pipe failed"); return;}


        string cmd1 = _trim(line.substr(0, idx1));
        string cmd2 = _trim(line.substr(idx1 + 2));//skip |&

        int fdErrBackup=dup(fileno(stderr));
        if (fdErrBackup == -1) { perror("smash error: dup failed"); close(fd[0]); close(fd[1]); return; }
        dup2(fd[1],fileno(stderr));
        close(fd[1]);
        sh.executeCommand(cmd1.c_str(),"");

        dup2(fdErrBackup,fileno(stderr));
        close(fdErrBackup);

        int fdInBackup=dup(fileno(stdin));
        dup2(fd[0],fileno(stdin));
        close(fd[0]);
        sh.executeCommand(cmd2.c_str(),"");


        dup2(fdInBackup,fileno(stdin));
        close(fdInBackup);

    }
    else if(idx2!=string::npos) {
        int fd[2];
        if(pipe(fd)==-1) { perror("smash error: pipe failed"); return; }

        string cmd1 = _trim(line.substr(0, idx2));;
        string cmd2 = _trim(line.substr(idx2+1));

        int fdOutBackup=dup(fileno(stdout));
        dup2(fd[1],fileno(stdout));
        close(fd[1]);
        sh.executeCommand(cmd1.c_str(),"");

        dup2(fdOutBackup,fileno(stdout));
        close(fdOutBackup);

        int fdInBackup=dup(fileno(stdin));
        if (fdInBackup == -1) { perror("smash error: dup failed"); close(fd[0]); return; }
        dup2(fd[0],fileno(stdin));
        close(fd[0]);

        sh.executeCommand(cmd2.c_str(),"");

        dup2(fdInBackup,fileno(stdin));
        close(fdInBackup);

    }

}



long long DiskUsageCommand::compute_disk_usage_blocks(string path) {
    struct stat st;
    if(lstat(path.c_str(), &st) == -1)
        return 0;

    long long total_blocks = (long long)st.st_blocks;

    if (!S_ISDIR(st.st_mode)) {
        return total_blocks;
    }

    int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd == -1)
        return total_blocks;// couldn't open directory; return what we have

    const int BUF_SIZE = 16 * 1024;
    std::vector<char> buf(BUF_SIZE);

    for(;;) {
        int nread = (int)syscall(SYS_getdents64, fd, buf.data(), BUF_SIZE);
        if(nread == -1 || nread == 0) {
            break;
        }
        if (nread == 0) {
            // end of directory
            break;
        }

        int bpos = 0;
        while(bpos<nread) {
            struct linux_dirent64 *d = (struct linux_dirent64 *)(buf.data() + bpos);
            string name = d->d_name;
            if (name != "." && name != "..") {
                std::string child;
                if (path == "/") child = "/" + name;
                else child = path + "/" + name;

                total_blocks += compute_disk_usage_blocks(child);
            }
            if (d->d_reclen == 0) {
                // safety: avoid infinite loop if reclen is broken
                break;
            }
            bpos +=d->d_reclen;
        }
    }
    close(fd);
    return total_blocks;
}
long long DiskUsageCommand::compute_disk_usage_kb(std::string path) {
    // compute total 512-byte blocks for path and everything under it (no symlink following)
    long long total_blocks = compute_disk_usage_blocks(path);

    // Convert 512-byte blocks to kilobytes (1 KB = 1024 bytes).
    // total_bytes = total_blocks * 512
    // total_kb = ceil(total_bytes / 1024) = ceil(total_blocks * 512 / 1024)
    // Because 1024 / 512 == 2, this simplifies to: total_kb = (total_blocks + 1) / 2
    long long total_kb = (total_blocks + 1) / 2;

    return total_kb;
}

void DiskUsageCommand::execute() {
    string path;
    if(argc>2) {
        cerr<<"smash error: du: too many arguments"<<endl;
        return;
    }
    if(argc==2) {
        path = args[1];
    }
    else {
        char cwd[COMMAND_MAX_LENGTH];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) path = cwd;
        else path = "."; // fallback
    }
    long long kb = compute_disk_usage_kb(path);
    cout<<"Total disk usage: " << kb << " KB"<<endl;
}


void WhoAmICommand::execute() {
    uid_t uid = getuid();
    gid_t gid = getgid();

    //string uid_str, gid_str;
    std::string uid_str = std::to_string((unsigned long)uid);
    std::string gid_str = std::to_string((unsigned long)gid);
    std::string username = "";
    std::string userHome = "";


    int fd = open("/etc/passwd",O_RDONLY);
    const size_t BUF_SZ = 4096;
    char buff[BUF_SZ];
    std::string acc;
    ssize_t  nread;
    while ((nread = read(fd, buff, BUF_SZ)) > 0) {
        acc.append(buff, static_cast<size_t>(nread));
        size_t nl;
        while ((nl = acc.find('\n')) != std::string::npos) {
            string line = acc.substr(0, nl);
            acc.erase(0, nl + 1);
            if (line.empty() || line[0] == '#') continue;

            vector<std::string> fields;
            size_t  start = 0;
            while(true) {
                size_t colon = line.find(':', start);
                if (colon == std::string::npos) {
                    fields.push_back(line.substr(start));
                    break;
                } else {
                    fields.push_back(line.substr(start, colon - start));
                    start = colon + 1;
                }
            }

            if(fields.size()>=6) {
                if (fields[2] == uid_str) {
                    username = fields[0];
                    // fields[3] is gid from passwd — we ignore matching gid, we print kernel gid too
                    userHome = fields[5];
                }
            }
        }
        if (!username.empty()) break; // found already
    }
    close(fd);
    std::cout << username<<std::endl;
    std::cout << uid_str << std::endl;
    std::cout << gid_str << std::endl;
    std::cout << userHome << std::endl;

}



//unsetenv
void UnSetEnvCommand::execute() {
    if(argc==1) {
        cerr<<"smash error: unsetenv: not enough arguments"<<endl;
        return;
    }

    string path = "/proc/self/environ";
    int fd = open(path.c_str(), O_RDONLY);
    if(fd==-1) {
        perror("smash error: open failed");
        return;
    }

    ssize_t  nread;
    const size_t BUF_SZ = 4096;
    char buff[BUF_SZ];
    std::string acc;
    vector<string> envEntries;

    while((nread = read(fd,buff,BUF_SZ)) > 0) {
        if(nread == -1)
            perror("smash error: read failed");
        acc.append(buff, static_cast<size_t>(nread));
        size_t nl;
        while ((nl = acc.find('\0')) != std::string::npos){
            string envVar = acc.substr(0, nl);
            acc.erase(0, nl + 1);
            if(!envVar.empty())
                envEntries.push_back(envVar);
        }
    }
    close(fd);

    set<string> presentNames;
    for(const auto& env : envEntries) {
        size_t eq = env.find('=');
        if(eq!=std::string::npos &&eq>0)
            presentNames.insert(env.substr(0, eq));
    }


    char** env = __environ;
    for(int i = 1; i < argc; i++) {
        string name = args[i];
        if(presentNames.find(name)==presentNames.end() ) {
            cerr<<"smash error: unsetenv: "<<name<<" does not exist"<<endl;
            return;
        }

        name+="=";
        for(int j=0;env[j]!=nullptr;++j) {
            if(strncmp(env[j], name.c_str(),name.size())==0) {
                int k = j;
                //remove and shift
                while(env[k]!=nullptr) {
                    env[k]=env[k+1];
                    k++;
                }
                break;
            }
        }
    }


}

ssize_t SysInfoCommand::safe_read_all(int fd, char* buf, size_t bufsize) {
    ssize_t total = 0;
    while (total < (ssize_t)bufsize) {
        ssize_t r = read(fd, buf + total, bufsize - (size_t)total);
        if (r < 0) return -1;
        if (r == 0) break;
        total += r;
    }
    // ensure null-terminated if space
    if (total < (ssize_t)bufsize) buf[total] = '\0';
    else buf[bufsize - 1] = '\0';
    return total;
}

long long SysInfoCommand::parse_positive_decimal(const char *s) {
    if (!s) return -1;
    long long v = 0;
    size_t i = 0;
    // skip spaces
    while (s[i] == ' ' || s[i] == '\t') ++i;
    if (s[i] < '0' || s[i] > '9') return -1;
    for (; s[i]; ++i) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            v = v * 10 + (c - '0');
        } else break;
    }
    return v;
}


//sysinfo
void SysInfoCommand::execute() {
    const int SMALL=512;
    char buf[SMALL+1];

    //system
    int fd = open("/proc/sys/kernel/ostype", O_RDONLY);
    if(fd==-1) {
        perror("smash error: open failed");
        return;
    }

    ssize_t r = safe_read_all(fd, buf, SMALL);
    close(fd);
    if (r > 0 && buf[r-1] == '\n') buf[r-1] = '\0';
    cout<<"System: "<<buf<<endl;


    //hostname
    fd = open("/proc/sys/kernel/hostname", O_RDONLY);
    if(fd==-1) {
        perror("smash error: open failed");
        return;
    }
    r = safe_read_all(fd, buf, SMALL);
    close(fd);
    if (r > 0 && buf[r-1] == '\n') buf[r-1] = '\0';
    cout<<"Hostname: "<<buf<<endl;


    //kernel
    fd = open("/proc/sys/kernel/osrelease", O_RDONLY);
    if(fd==-1) {
        perror("smash error: open failed");
        return;
    }
    r = safe_read_all(fd, buf, SMALL);
    close(fd);
    if (r > 0 && buf[r-1] == '\n') buf[r-1] = '\0';
    cout<<"Kernel: "<<buf<<endl;



    //architecture
    cout<<"Architecture: x86_64"<<endl;


    //boot
    const int BIG=8192;
    char bigbuf[BIG+1];
    fd = open("/proc/stat", O_RDONLY);
    ssize_t got = safe_read_all(fd, bigbuf, BIG);
    close(fd);
    bigbuf[ (got < (ssize_t)BIG) ? (size_t)got : BIG-1 ] = '\0';
    // find "btime " line
    std::string bigb(bigbuf, (got > 0) ? (size_t)got : 0u);
    size_t i = bigb.find("btime ");
    size_t start = i + 6; // length of "btime "
    while (start < bigb.size() && (bigb[start] == ' ' || bigb[start] == '\t')) ++start;
    size_t end = start;
    while (end < bigb.size() && bigb[end] != '\n') ++end;
    std::string btimeString = bigb.substr(start, end - start);
    long long btime_ll = parse_positive_decimal(btimeString.c_str());
    time_t boot_time = (time_t)btime_ll;

    //format boot time
    struct tm tmres;
    localtime_r(&boot_time, &tmres);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tmres);
    cout<<"Boot Time: "<<timestr<<endl;


}


vector<string> USBInfoCommand::list_dir_names(const char* path) {
    vector<string> names;
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd == -1) return names;
    const int BUF_SIZE = 8192;
    char buf[BUF_SIZE];
    for(;;) {
        int nread = (int)syscall(SYS_getdents64, fd, buf, BUF_SIZE);
        if (nread == -1) {
            close(fd);
            return {};
        }
        if (nread == 0) break;
        int bpos = 0;
        while (bpos < nread) {
            linux_dirent64 *d = (linux_dirent64*)(buf + bpos);
            string name(d->d_name);
            names.push_back(name);
            bpos+=d->d_reclen;
        }
    }
    close(fd);
    return names;
}

void USBInfoCommand::execute() {
    const char *base = "/sys/bus/usb/devices";
    vector<string> entries= list_dir_names(base);
    struct DeviceEntry {
        int devnum;
        std::string vendor;
        std::string product;
        std::string manufacturer;
        std::string product_name;
        std::string maxpower; // numeric value in mA or empty
    };
    vector<DeviceEntry> devices;

    for(const string& name: entries) {
        if (name.empty()) continue;
        if (name[0] == '.') continue;        // skip . and ..
        if (name.find(':') != string::npos) continue; // skip "1-1:1.0" interface entries
        string devdir = string(base)+'/'+name;
        string devnum_str = read_sysfs_file(devdir, "devnum");
        if(devnum_str.empty()) continue; //not a device
        int devnum = 0;
        try {
            devnum = std::stoi(devnum_str);
        } catch (...) {
            continue;
        }
        DeviceEntry de;
        de.devnum = devnum;
        string idv = read_sysfs_file(devdir, "idVendor");
        string idp = read_sysfs_file(devdir, "idProduct");
        de.vendor = idv.empty() ? "N/A" : idv;
        de.product = idp.empty() ? "N/A" : idp;

        string manu = read_sysfs_file(devdir, "manufacturer");
        string prodname = read_sysfs_file(devdir, "product");
        de.manufacturer = manu.empty() ? "N/A" : manu;
        de.product_name = prodname.empty() ? "N/A" : prodname;

        string bMaxPower = read_sysfs_file(devdir, "bMaxPower");
        if (bMaxPower.empty()) {
            de.maxpower = "N/A";
        } else {
            // extract digits and append "mA"
            string digits;
            for (char c : bMaxPower) {
                if (c >= '0' && c <= '9') digits.push_back(c);
            }
            if (digits.empty()) de.maxpower = "N/A";
            else de.maxpower = digits + "mA";
        }

        devices.push_back(de);


    }
    if (devices.empty()) {
        cout << "smash error: usbinfo: no USB devices found" << endl;
        return;
    }
    std::sort(devices.begin(), devices.end(), [](const DeviceEntry &a, const DeviceEntry &b){
       return a.devnum < b.devnum;
   });


    for (const auto &dev : devices) {
        cout << "Device " << dev.devnum
             << ": ID " << (dev.vendor.empty() ? "N/A" : dev.vendor)
             << ":" << (dev.product.empty() ? "N/A" : dev.product)
             << " " << (dev.manufacturer.empty() ? "N/A" : dev.manufacturer)
             << " " << (dev.product_name.empty() ? "N/A" : dev.product_name)
             << " MaxPower: " << (dev.maxpower.empty() ? "N/A" : dev.maxpower)
             << endl;
    }

}







/*
    ssize_t bytes_read;
    int BUFFER_SIZE = 256;
    int fd1,fd2,fd3,fd4;


    if( (fd1 = open("/proc/sys/kernel/ostype", O_RDONLY)) < 0) {
        perror("smash error: open failed");
        return;
    }
    char buffer1[BUFFER_SIZE];
    bytes_read = read(fd1, buffer1, BUFFER_SIZE);
    buffer1[bytes_read] = '\0';
    close(fd1);
    cout<<"System: "<<buffer1<<endl;



    if( (fd2 = open("/proc/sys/kernel/hostname", O_RDONLY)) < 0) {
        perror("smash error: open failed");
        return;
    }
    char buffer2[BUFFER_SIZE];
    bytes_read = read(fd2, buffer2, BUFFER_SIZE);
    buffer2[bytes_read] = '\0';
    close(fd2);
    cout<<"Hostname: "<<buffer2<<endl;



    if( (fd3 = open("/proc/sys/kernel/osrelease", O_RDONLY)) < 0) {
        perror("smash error: open failed");
        return;
    }
    char buffer3[256];
    ssize_t n, total = 0;
    while((n = read(fd2, buffer3+total, sizeof(buffer3)-1-total))>0  ){
        total+=n;
        if (total >= sizeof(buffer3) - 1) break;
    }
    close(fd3);
    buffer2[total] = '\0';
    cout<<"Kernel: "<<buffer3<<endl;

    cout<<"Architecture: x86_64"<<endl;



    if( (fd4 = open("/proc/stat", O_RDONLY)) < 0) {
        perror("smash error: open failed");
        return;
    }
    const size_t BUF_SZ = 4096;
    char buffer4[BUF_SZ], nread;
    std::string content;
    vector<string> lines;
    while ((nread = read(fd4, buffer4, BUF_SZ)) > 0) {
        content.append(buffer4, static_cast<size_t>(nread));
    }
    close(fd4);

    size_t pos = 0;
    const string target = "btime ";
    string btime;
    while (pos <= content.size()) {
        size_t nl = content.find('\n', pos);
        size_t line_end = (nl == std::string::npos) ? content.size() : nl;
        size_t line_len = (line_end > pos) ? (line_end - pos) : 0;
        if (line_len >= target.size()) {
            if (content.compare(pos, target.size(), target) == 0) {
                size_t i = pos + target.size();
                while (i < line_end && (content[i] == ' ' || content[i] == '\t')) ++i;
                if (i >= line_end) break;
                btime = content.substr(i, line_len);
                break;
            }
        }
    }
    long long btimell = atoll(btime.c_str());

    std::time_t current_time = std::time(nullptr);
    std::time_t boot_time = current_time - (std::time_t)btimell;

    // Convert boot_time to a human-readable date string
    char buf[80];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&boot_time));
    cout<<"Boot Time: "<<buf<<endl;

}

*/
        /*
    struct utsname u;
    if (uname(&u) != 0) {
        perror("smash error: uname failed");
        return;
    }
    cout<<"System: "<< u.sysname<< endl;
    cout<<"Hostname: "<< u.nodename<< endl;
    cout<<"Kernel: "<< u.release<< endl;
    cout<<"Architecture: "<< u.machine<< endl;

    struct sysinfo si;
    if(sysinfo(&si)!=0) {
        perror("smash error: sysinfo failed");
        return;
    }
    struct timespec now;
    if(clock_gettime(&now,CLOCK_REALTIME)!=0) {
        perror("smash error: clock_gettime failed");
        return;
    }
    struct tm tm;
    if (localtime_r(&boot_time, &tm) == nullptr) {
        perror("smash error: localtime_r failed");
        return;
    }

    time_t boot_time = static_cast<time_t>(now.tv_sec) - static_cast<time_t>(si.uptime);

    char timestr[64];
    if (strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm) != 0) {
        perror("smash error: strftime failed");
        return;
    }

    std::strncpy(timestr, "(time format error)", sizeof(timestr));
    timestr[sizeof(timestr)-1] = '\0';

    std::cout << "Boot Time: " << timestr << '\n';
    */
