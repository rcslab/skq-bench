import json
import numpy as np

class memdata:
    dat = None
    qps = None
    def __init__(self):
        self.dat = {}
        self.qps = 0

    def to_string(self):
        ret = "Throughput: " + str(self.qps) + "\n" + json.dumps(self.dat)
        return ret

def parse_mut_output(output):
    ret = memdata()
    succ_qps = False
    succ_read = False
    table = [None, "avg", "std", "min", "5th", "10th", "50th", "90th", "95th", "99th"]
    table_legacy = [None, "avg", "std", "min", "5th", "10th", "90th", "95th", "99th"]
    for line in output.splitlines():
        if line.find("Total QPS") != -1:
            spl = line.split()
            if len(spl) == 7:
                ret.qps = float(spl[3])
                succ_qps = True
            else:
                break
        elif line.find("read") != -1:
            spl = line.split()
            if len(spl) == 10:
                for i in range(1, len(spl)):
                    ret.dat[table[i]] = float(spl[i])
                succ_read = True
            elif len(spl) == 9:
                for i in range(1, len(spl)):
                    ret.dat[table_legacy[i]] = float(spl[i])
                succ_read = True
            else:
                break
    
    if not (succ_qps and succ_read):
        raise Exception("Failed to parse data")

    return ret

def parse_mut_sample(fn):
    f = open(fn, "r")
    qps = []
    lat = []
    lines = f.readlines()
    for line in lines:
        entry = line.split()
        if len(entry) != 2:
            raise Exception("Unrecognized line: " + line)
        qps.append(float(entry[0]))
        lat.append(float(entry[1]))
    f.close()
    return qps, lat


# generate mutilate output format
def build_mut_output(lat_arr, qps_arr):

	output = '{0: <10}'.format('#type') + '{0: >10}'.format('avg') + '{0: >10}'.format('std') + \
				      '{0: >10}'.format('min') + '{0: >10}'.format('5th') + '{0: >10}'.format('10th') + \
					  '{0: >10}'.format('50th') + '{0: >10}'.format('90th')  + '{0: >10}'.format('95th') + '{0: >10}'.format('99th') + "\n"
	
	output += '{0: <10}'.format('read') + '{0: >10}'.format("{:.1f}".format(np.mean(lat_arr))) + ' ' + \
                      '{0: >10}'.format("{:.1f}".format(np.std(lat_arr))) + ' ' + \
				      '{0: >10}'.format("{:.1f}".format(np.min(lat_arr))) + ' ' + \
					  '{0: >10}'.format("{:.1f}".format(np.percentile(lat_arr, 5))) + ' ' + \
					  '{0: >10}'.format("{:.1f}".format(np.percentile(lat_arr, 10))) + ' ' + \
					  '{0: >10}'.format("{:.1f}".format(np.percentile(lat_arr, 50))) + ' ' + \
					  '{0: >10}'.format("{:.1f}".format(np.percentile(lat_arr, 90))) + ' ' + \
					  '{0: >10}'.format("{:.1f}".format(np.percentile(lat_arr, 95))) + ' ' + \
					  '{0: >10}'.format("{:.1f}".format(np.percentile(lat_arr, 99))) + ' ' + "\n" \

	output += "\n" + "Total QPS = " + "{:.1f}".format(np.mean(qps_arr)) + " (0 / 0s)"

	return output
