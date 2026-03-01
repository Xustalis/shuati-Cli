import subprocess
import sys

def run(cmd):
    try:
        return subprocess.check_output(cmd, shell=True).decode('utf-8').strip()
    except subprocess.CalledProcessError:
        return "Error"

def check_branches():
    try:
        raw_branches = run("git branch -r")
        if raw_branches == "Error":
            print("Error: Not a git repository or git command failed.")
            return

        branches = [b.strip() for b in raw_branches.splitlines() if "->" not in b]
        
        print("# Branch Health Report")
        print("| Branch | Behind Main | Ahead Main | Last Commit | Status |")
        print("|---|---|---|---|---|")
        
        for branch in branches:
            branch_name = branch.replace("origin/", "")
            if branch_name == "HEAD": continue
            
            behind = run(f"git rev-list --count {branch}..origin/main")
            ahead = run(f"git rev-list --count origin/main..{branch}")
            last_commit = run(f"git log -1 --format=%cd --date=short {branch}")
            
            status = "🟢 Healthy"
            if behind != "Error" and int(behind) > 0:
                status = "🟡 Outdated"
            if behind != "Error" and int(behind) > 10:
                 status = "🔴 Stale"
            if behind == "Error":
                status = "🔴 Error"
                 
            print(f"| {branch_name} | {behind} | {ahead} | {last_commit} | {status} |")
            
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    check_branches()
