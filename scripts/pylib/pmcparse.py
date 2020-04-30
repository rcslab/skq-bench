import json
import numpy as np

class pmcdata:
    def __init__(self):
        self.data = []
        self.qps = 0

# data = [[name, absolute, rel, rel unit], ...]

def parseline(line: str):
    start_idx = 1
    lines = line.split()
    end_idx = len(lines)
    tmp = 0
    
    name = ""
    unit = None
    rel = 0.0
    val = float(lines[0])

    for each in lines:
        if each == "#":
            end_idx = tmp
            break
        tmp = tmp + 1
    
    for i in range(start_idx, end_idx, 1):
        name = name + ("_" if len(name) > 0 else "") + lines[i]
    
    if end_idx != len(lines):
        # rel and unit exists
        if (lines[end_idx + 1].endswith("%")):
            rel = float(lines[end_idx+1][:-1])
            unit = "%"
        else:
            rel = float(lines[end_idx+1])
            unit = lines[end_idx+2]

    dat = []
    dat.append(name)
    dat.append(val)
    dat.append(rel)
    dat.append(unit)
    return dat
        

def parse_pmc_output(output: str):
    ret = pmcdata()
    
    for line in output.splitlines():
        ret.data.append(parseline(line))

    return ret
