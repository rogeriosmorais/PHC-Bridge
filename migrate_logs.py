import re
import os

files = [
    r"F:\NewEngine-AgentB\PhysAnimUE5\Plugins\PhysAnimPlugin\Source\PhysAnimPlugin\Private\PhysAnimBalanceQuietHandoff.cpp",
    r"F:\NewEngine-AgentB\PhysAnimUE5\Plugins\PhysAnimPlugin\Source\PhysAnimPlugin\Private\PhysAnimBalanceReadyTransition.Certification.cpp",
    r"F:\NewEngine-AgentB\PhysAnimUE5\Plugins\PhysAnimPlugin\Source\PhysAnimPlugin\Private\PhysAnimBalanceReadyTransition.Core.cpp"
]

pattern = re.compile(r'UE_LOG\s*\(\s*([a-zA-Z0-9_]+)\s*,\s*([a-zA-Z0-9_]+)\s*,')

for path in files:
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # insert include if missing
    if '#include "PhysAnimLogger.h"' not in content:
        content = re.sub(r'(#include "[^"]+")', r'\1\n#include "PhysAnimLogger.h"', content, count=1)
        
    new_content = pattern.sub(r'PHYSANIM_LOG_RATE_LIMITED(\1, \2, 1.0f,', content)
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    print(f"Updated {path}")
