import os

result_dir = "results.d"
output_dir = "graphs.d"

print("Creating directories...")
os.system("rm -rf " + output_dir + "; mkdir " + output_dir)

files = os.listdir(result_dir)

for i in range(len(files)):
    each = files[i]
    if each.endswith(".t"):
        print("Processing " + each)
        os.system("python3 ../../graph/thread_scale.py " + result_dir + "/" + each + " " + output_dir + "/" + each + ".png")
