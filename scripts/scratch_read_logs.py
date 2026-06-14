import os
import glob

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir))
DEFAULT_LOG_DIR = os.path.join(REPO_ROOT, "PhysAnimUE5", "Saved", "Logs")

files = glob.glob(os.path.join(DEFAULT_LOG_DIR, "*.log"))
if files:
    latest_file = max(files, key=os.path.getmtime)
    print(f"Reading: {latest_file}")
    
    # Try different encodings
    for encoding in ['utf-16', 'utf-8', 'latin-1']:
        try:
            with open(latest_file, 'r', encoding=encoding) as file:
                lines = file.readlines()
            print(f"Successfully read with {encoding}: {len(lines)} lines")
            
            # Print matching lines
            matches = []
            for line in lines:
                if any(kwd in line for kwd in ["ENTRY", "rejected", "PROOF", "StandingProof", "balance_start", "Fail-stop"]):
                    matches.append(line.strip())
            
            print(f"Found {len(matches)} matching lines:")
            for m in matches[-50:]:  # show last 50 matches
                print(m)
            break
        except Exception as e:
            print(f"Failed with {encoding}: {e}")
else:
    print("No log files found.")
