
#include <unistd.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

#include <celestis/debug.h>
#include <celestis/command.h>
#include <celestis/terminal.h>
#include <celestis/termui.h>
#include <celestis/shell.h>

#include <celestis/OSD/dinode.h>
#include <celestis/OSD/diskosd.h>
#include <celestis/OSD/filedisk.h>

using namespace std;

static vector<Disk*> disks;
static DiskOSD *os = nullptr;
static CVNode *v = nullptr;

void
cmd_diskadd(IShell *s, vector<string> args)
{
    if (args.size() != 2) {
        s->writeLine("Usage: diskadd SIZE_IN_MB");
        return;
    }

    uint64_t size = stoll(args[1]) * (1024 * 1024);

    disks.push_back(new FileDisk(size));
}
DECLCMD(diskadd, "Add a disk to the storage pool", "");

void
cmd_mount(IShell *s, vector<string> args)
{
    if (args.size() != 1) {
        s->writeLine("Usage: mount");
        return;
    }

    if (os != nullptr) {
        s->writeLine("Already mounted!");
        return;
    }

    os = new DiskOSD();
    os->initialize(disks);
}
DECLCMD(mount, "Create OSD storage pool", "");

void
cmd_inode(IShell *s, vector<string> args)
{
    if (args.size() != 2) {
        s->writeLine("Usage: inode INODE#");
        return;
    }

    uint64_t inode = stoll(args[1]);

    v = os->open(inode);
    if (v == nullptr) {
        s->writeLine("Cannot find inode#: " + args[1]);
    }
}
DECLCMD(inode, "Select an inode", "");

void
cmd_inodeshow(IShell *s, vector<string> args)
{
    if (args.size() != 1) {
        s->writeLine("Usage: inodeshow");
        return;
    }

    if (v == nullptr) {
        s->writeLine("No inode selected!");
        return;
    }

    std::string str = v->to_string();
    s->writeLine(str);
}
DECLCMD(inodeshow, "Show inode metadata", "");

void
cmd_inoderead(IShell *s, vector<string> args)
{
}
DECLCMD(inoderead, "Read an inode data", "");

int
main(int argc, const char *argv[])
{
    string str;
    TermUI t = TermUI(0);
    Shell s = Shell(t);

    t.init();
    s.setTitle(" Object Storage Device Debugger");
    s.setStatus("");
    s.process();
}

