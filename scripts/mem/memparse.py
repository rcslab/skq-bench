import json

class memdata:
    dat = None
    qps = None
    def __init__(self):
        self.dat = {}
        self.qps = 0

    def to_string(self):
        ret = "Throughput: " + str(self.qps) + "\n" + json.dumps(self.dat)
        return ret


def parse(output):
    ret = memdata()
    succ_qps = False
    succ_read = False
    table = [None, "avg", "std", "min", "5th", "10th", "90th", "95th", "99th"]
    for line in output.splitlines():
        if line.find("Total QPS") != -1:
            spl = line.split()
            if len(spl) == 7:
                ret.qps = float(spl[3])
                succ_qps = True
            else:
                print("invalid QPS line: " + line)
                break
        elif line.find("read") != -1:
            spl = line.split()
            if len(spl) == 9:
                for i in range(1, len(spl)):
                    ret.dat[table[i]] = float(spl[i])
                succ_read = True
            else:
                print("invalid read line: " + line)
                break
    
    if not (succ_qps and succ_read):
        raise Exception("Failed to parse data")

    return ret
