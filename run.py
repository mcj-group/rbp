import itertools
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os
import seaborn as sns
import subprocess
from subprocess import TimeoutExpired
import sys

proc = [70] #[1, 2, 6, 10, 20, 30, 40, 50, 60, 70]

# all
#algorithms = ["concurrent-unfair", "concurrent-fair",
#              "relaxed-unfair", "relaxed-fair",
#              "splash-unfair", "splash-fair",
#              "relaxed-splash-unfair", "relaxed-splash-fair",
#              "relaxed-smart-splash-unfair", "relaxed-smart-splash-fair",
#              "relaxed-priority-fair", "smart-relaxed-priority-fair",
#              "weight-decay-fair"]

# small
algorithms = ["relaxed-unfair", "relaxed-fair",
              "splash-unfair", "splash-fair",
              "relaxed-splash-unfair", "relaxed-splash-fair",
              "relaxed-smart-splash-unfair", "relaxed-smart-splash-fair",
              "relaxed-priority-fair", "smart-relaxed-priority-fair",
              "weight-decay-fair"]
# splash
#algorithms = ["splash-unfair", "splash-fair", "relaxed-splash-unfair", "relaxed-splash-fair", "relaxed-smart-splash-unfair", "relaxed-smart-splash-fair"]
# fair
#algorithms = ["concurrent-fair", "relaxed-fair", "splash-fair", "relaxed-splash-fair", "relaxed-smart-splash-fair", "relaxed-priority-fair", "smart-relaxed-priority-fair"]
# ldpc
#algorithms = ["synchronous", "concurrent-unfair", "relaxed-unfair"]

# best
algorithms = [
#               "synchronous",
#               "concurrent-fair",
#               "relaxed-fair",
#               "smart-relaxed-priority-fair",
#               "weight-decay-fair",
               "splash-fair",
               "smart-splash-fair",
               "relaxed-splash-fair",
               "relaxed-smart-splash-fair",
               "randomr-splash-fair",
               "randomr-smart-splash-fair",
               "bucket-residual"
              ]
                
aliases = {
            "synchronous": "Synchronous",
            "concurrent-fair": "Coarse-Grained Residual",
            "relaxed-fair": "Relaxed Residual",
            "smart-relaxed-priority-fair": "Relaxed Priority",
            "weight-decay-fair": "Weight-decay",
            "splash-fair": "Splash",
            "smart-splash-fair": "Smart Splash",
            "relaxed-smart-splash-fair": "Relaxed Smart Splash",
            "randomr-splash-fair": "Random Splash",
            "randomr-smart-splash-fair": "Random Smart Splash",
            "bucket-residual": "Bucket Residual"
          }
# "synchronous"
# "smart-relaxed-priority-fair", 

relaxed_algorithms = ["relaxed-fair", "weight-decay-fair", "relaxed-smart-splash-fair", "smart-relaxed-priority-fair"]

forbidden_algorithms = []#"concurrent-fair"]

#algorithms = ["weight-decay-fair"]

inputs = [("ising", 300), ("potts", 300)]
#inputs = [("potts", 300)]
## ldpc
#inputs = [("ldpc", 30000)]
# deterministic_tree
#inputs = [("deterministic_tree", 1000000)]
inputs = [("ldpc", 30000), ("deterministic_tree", 1000000)]
#inputs = [("deterministic_tree", 1000000), ("ldpc", 30000), ("ising", 300), ("potts", 300)]
#inputs = [("deterministic_tree", 1000000), ("ising", 300), ("potts", 300), ("ldpc", 30000)]

#splash_h = [5, 10]
#splash_h = [5]
splash_h = [2, 5, 10]

def compile():
    os.system("ant compile")

def get_filename(algorithm, model, proc, h):
   arguments = "{}-{}-{}-{}-{}".format(algorithm, model[0], model[1], proc, h)
   return "out/logs/{}.txt".format(arguments)

def get_filename(prefix, algorithm, model, proc, h):
   arguments = "{}-{}-{}-{}-{}".format(algorithm, model[0], model[1], proc, h)
   return "{}/{}.txt".format(prefix, arguments)

def run(T):
   if not os.path.isdir("out/logs"):
        os.makedirs("out/logs")
   for setting in itertools.product(proc, algorithms, inputs):
       hs = [0]
       if "splash" in setting[1]:
           hs = splash_h
       for h in hs:
          outfile = get_filename(setting[1], setting[2], setting[0], h)
          arguments = "{} {} {} {} {}".format(setting[1], setting[2][0], setting[2][1], setting[0], h)
          commandline = "java -XX:+UseRTMLocking -cp bin bp.testing.Main {}".format(arguments)

          if os.path.isfile(outfile):
              os.remove(outfile)
          print(commandline)
          for i in range(T):
              try:
                  subprocess.call("{} >> {}".format(commandline, outfile), shell="True", timeout=300)
              except TimeoutExpired:
                  print("Didn't get into 120 seconds timeout")
                                        
def large_run(T, execute):
    prefix = "out/large_logs"
    if not os.path.isdir(prefix):        
        os.makedirs(prefix)             
#    inputs = [("ising", 1000), ("potts", 1000)]
#    inputs =[("ldpc", 300000), ("ldpc", 150000)]
#    inputs = [("ldpc", 300000), ("ldpc", 150000), ("deterministic_tree", 10000000)]
    inputs = [("ising", 1000), ("potts", 1000), ("ldpc", 300000), ("deterministic_tree", 10000000)]
#    inputs = [("ldpc", 1000000)]
    algorithms = [
#                  ("synchronous", 70, 0), ("concurrent-fair", 70, 0),
#                  ("residual", 1, 0),
#                  ("relaxed-fair", 70, 0),
#                  ("weight-decay-fair", 70, 0), ("smart-relaxed-priority-fair", 70, 0),
#                  ("splash-fair", 70, 10), ("relaxed-splash-fair", 70, 10), ("randomr-splash-fair", 70, 10),
#                  ("smart-splash-fair", 70, 10), ("relaxed-smart-splash-fair", 70, 10), ("randomr-smart-splash-fair", 70, 10),
#                  ("splash-fair", 70, 2), ("relaxed-splash-fair", 70, 2), ("randomr-splash-fair", 70, 2),
#                  ("smart-splash-fair", 70, 2), ("relaxed-smart-splash-fair", 70, 2), ("randomr-smart-splash-fair", 70, 2),

#                  ("synchronous", 70, 0),
#                  ("concurrent-fair", 70, 0),
#                  ("splash-fair", 70, 2),
#                  ("splash-fair", 70, 10),
#                  ("randomr-splash-fair", 70, 2),
#                   ("randomr-splash-fair", 70, 10),
#                  ("relaxed-fair", 70, 0),
#                  ("weight-decay-fair", 70, 0),
#                  ("smart-relaxed-priority-fair", 70, 0),
#                  ("relaxed-smart-splash-fair", 70, 2),
#                 ("relaxed-smart-splash-fair", 70, 10),
                  ("bucket-residual", 70, 0.1),

#                  ("synchronous", 20, 0),
#                  ("randomr-splash-fair", 20, 2),
#                  ("randomr-splash-fair", 20, 10),
#                  ("splash-fair", 20, 2),
#                  ("splash-fair", 20, 10),
#                  ("smart-splash-fair", 20, 2),
#                  ("smart-splash-fair", 20, 10),
#                  ("relaxed-fair", 20, 0),

#                  ("synchronous", 35, 0),
#                  ("randomr-splash-fair", 35, 2),
#                  ("randomr-splash-fair", 35, 10),
#                  ("splash-fair", 35, 2),
#                  ("splash-fair", 35, 10),
#                  ("smart-splash-fair", 35, 2),
#                  ("smart-splash-fair", 35, 10),
#                  ("relaxed-fair", 35, 0),

#                  ("synchronous", 70, 0),
#                  ("randomr-splash-fair", 70, 2),
#                  ("randomr-splash-fair", 70, 10),
#                  ("splash-fair", 70, 2),
#                  ("splash-fair", 70, 10),
#                  ("smart-splash-fair", 70, 2),
#                  ("smart-splash-fair", 70, 10),
#                  ("relaxed-fair", 70, 0),

                ]

    if execute:
        for setting in itertools.product(algorithms, inputs):
            outfile = get_filename(prefix, setting[0][0], setting[1], setting[0][1], setting[0][2])
            arguments = "{} {} {} {} {}".format(setting[0][0], setting[1][0], setting[1][1], setting[0][1], setting[0][2])
            commandline = "java -XX:+UseRTMLocking -Xmx500G -cp bin bp.testing.Main {}".format(arguments)

            if os.path.isfile(outfile):
                os.remove(outfile)
            print(commandline)

            for i in range(T):
                try:
                    subprocess.call("{} >> {}".format(commandline, outfile), shell="True", timeout=600)
                except TimeoutExpired:
                    print("Didn't get into 300 seconds timeout")
    else:
        key = "updates" 
        for model in inputs:
            print(model)
            baseline = keyFromFile(get_filename(prefix, "residual", model, 1, 0), key)
            line = "& {:.2f}".format(baseline / 1000 / 1000)
            for algo in algorithms:
                outfile = get_filename(prefix, algo[0], model, algo[1], algo[2])
                if not os.path.isfile(outfile):
                    continue

                result = keyFromFile(outfile, key)
                if result == 0:
                    line += " & $\\infty$"
                else:
                    line += " & {:.3f}x".format(1. * result / baseline)
            print(line)

def keyFromFile(filename, key):
    f = open(filename, 'r')
    total = 0
    number = 0
    for line in f.readlines():
        if key in line.lower():
            if len(line.split(":")) <= 1:
                continue
            total += int(line.split(":")[1].strip())
            number += 1
    if number == 0:
        return 0
    return total / number

def algorithmName(algorithm, h):
   if "splash" in algorithm:
       return "{} H={}".format(aliases[algorithm], h)
   else:
       return aliases[algorithm]

def flip(items, ncol):
    return itertools.chain(*[items[i::ncol] for i in range(ncol)])

def plot(fairness, key):
   if not os.path.isdir("out/plots"):
       os.makedirs("out/plots")

#   sns.set_palette(sns.color_palette("husl", 12))
#   sns.set_palette(sns.color_palette("hls", 12))
   if fairness == "all":
       sns.set_palette(sns.color_palette("Paired"))
   else:
       colors = ['blue', 'green', 'red', 'cyan', 'magenta', 'black', 'purple', 'pink', 'brown', 'orange', 'teal', 'coral', 'lightblue', 'lavender', 'turquoise', 'darkgreen', 'tan', 'salmon', 'gold', 'yellow', 'lime']
       sns.set_palette(sns.xkcd_palette(colors))

   markers = ['o', 's', '*', 'x', '+', '^', 'D']
   
   for data in inputs:
       plot_id = 0
       print(data)
       plt.clf()
       fig = plt.figure()
       ax = fig.add_subplot(111)
#       ax.set_prop_cycle('color', plt.cm.Spectral(np.linspace(0, 1, 20))) #)
#       cm = plt.get_cmap('gist_rainbow')
#       ax.set_color_cycle([cm(1.*i / len(inputs)) for i in range(len(algorithms))])

       for hid in range(len(splash_h)):
           for algorithm in algorithms:
               if fairness == "fair" and "unfair" in algorithm:
                   continue
               if "splash" in algorithm:
                   h = splash_h[hid]
               else:
                   if hid == 0:
                       h = 0
                   else:
                       continue
               x = []
               y = []
               for proc in range(1, 80):
                   filename = get_filename(algorithm, data, proc, h)
                   if not os.path.isfile(filename):
                       continue
                   result = keyFromFile(filename, key)
                   if result == 0:
                       continue
                   if result >= 40000:
                       continue
                   x.append(proc)
                   y.append(1.0 * result / 1000)
               if len(x) != 0:
# and algorithm not in forbidden_algorithms \
#                   and (algorithm != "relaxed-smart-splash-fair" or h != 5):
                   ax.plot(x, y, color=colors[plot_id % len(colors)], marker=markers[plot_id % len(markers)], linestyle='--' if algorithm in relaxed_algorithms else '-', label=algorithmName(algorithm, h))
               plot_id += 1

#               arguments = "{}-{}-{}-{}-{}".format(algorithm, data[0], data[1], 143, h)
#               y = timeFromFile("out/logs/{}.txt".format(arguments))
#               print("{:30}: {} s".format(algorithmName(algorithm, h), y))
       if data[0] == "deterministic_tree":
           handles, labels = ax.get_legend_handles_labels()
       plt.legend()
       plt.xlabel("Number of processes")
       if key == "time":
            plt.ylabel("Time, s")
       elif key == "updates":
            plt.ylabel("Number of updates")
       plotname = "out/plots/{}-{}-{}.pdf".format(data[0], data[1], key)
       plt.savefig(plotname, bbox_inches='tight')

#   plt.clf()
#   fig_legend = plt.figure(figsize=(10, 1))
#   ax = fig_legend.add_subplot(111)
#   fig_legend.legend(flip(handles, 4), flip(labels, 4), loc='center', frameon=False, ncol=4)
#   ax.xaxis.set_visible(False)
#   ax.yaxis.set_visible(False)
#   fig_legend.savefig("out/plots/legend.pdf", bbox_inches='tight')

# subprocess.call(command, stdout=f, shell=True, stderr=subprocess.STDOUT, env=my_env)

def relaxed_table():
#   line = "\\begin{tabular}{|"
#   for i in range(len(proc) + 2):
#       line += "c|"
#   line += "}"
#   print(line)
#   print("\\hline")
#   
#   line = "Model & baseline"
#   for p in proc:
#       line += " & " + str(p)
#   line += "\\\\\\hline"
#   print(line)
#
#   for model in inputs:
#       line = model[0]
#       baseline = keyFromFile(get_filename("concurrent-fair", model, 1, 0), "updates")
#       line += " & " + str(baseline)
#       for p in proc:
#           real = keyFromFile(get_filename("relaxed-fair", model, p, 0), "updates")
#           percent = 100.0 * (real - baseline) / baseline
#           if percent >= 0:
#               line += " & {} (+{:.2f}\\%)".format(real, percent)
#           else:
#               line += " & {} ({:.2f}\\%)".format(real, percent)
#       line += "\\\\\\hline"
#       print(line)
#
#   print("\\end{tabular}")

   line = "\\begin{tabular}{|"
   for i in range(len(inputs) + 1):
       line += "c|"
   line += "}"
   print(line)
   print("\\hline")

   line = ""
   for model in inputs:
       line += " & " + model[0]
   line += "\\\\\\hline"
   print(line)

   line = "baseline"
   for model in inputs:
        baseline = keyFromFile(get_filename("concurrent-fair", model, 1, 0), "updates")
        line += " & " + str(baseline)
   line += "\\\\\\hline"
   print(line)

   for p in proc:
       line = str(p)
       for model in inputs:
           baseline = keyFromFile(get_filename("concurrent-fair", model, 1, 0), "updates")
           real = keyFromFile(get_filename("relaxed-fair", model, p, 0), "updates")
           percent = 100.0 * (real - baseline) / baseline
           if percent >= 0:
               line += " & +{:.2f}\\%".format(percent)
           else:
               line +=  "& {:.2f}\\%".format(percent)
       line += "\\\\\\hline"
       print(line)

def against_nonrelaxed_table():
    line = "\\begin{tabular}{|"
    for i in range(len(inputs) + 1):
        line += "c|"
    line += "}"
    print(line)
    print("\\hline")
 
    line = ""
    for model in inputs:
        line += " & " + model[0]
    line += "\\\\\\hline"
    print(line)
 
#    line = "RR"
#    for model in inputs:
#         baseline = keyFromFile(get_filename("relaxed-fair", model, 70, 0), "time")
#         line += " & " + str(baseline) + "s"
#    print(line)

    for p in proc:
        line = str(p)
        for model in inputs:
            baseline = keyFromFile(get_filename("relaxed-fair", model, p, 0), "time")

            best = float("inf")
            for hid in range(len(splash_h)):
                for algorithm in algorithms:
                    if algorithm in relaxed_algorithms:
                        continue
                    if algorithm == "synchronous":
                        continue
                    if "splash" in algorithm:
                        h = splash_h[hid]
                    else:
                        if hid == 0:
                            h = 0
                        else:
                            continue
                    filename = get_filename(algorithm, model, p, h)
                    if not os.path.isfile(filename):
                        continue
                    best = min(best, keyFromFile(filename, "time"))
            if best > baseline:
                line += " & \\textbf{{{:.2f}x}}".format(1.0 * best / baseline)
            else:
                line += " & {:.2f}x".format(1.0 * best / baseline)

#            percent = (best - baseline) / baseline
#            if percent >= 0:
#                line += " & \\textbf{{+{:.2f}}}\\%".format(percent)
#                line += " & \\textbf{{{:.2f}}x}\\%".format(percent)
#            else:
#                line += " & {:.2f}\\%".format(percent)
        line += "\\\\\\hline"
        print(line)

def speedup_table():
    line = "\\begin{tabular}{|"
    for i in range(len(proc) + 1):
        line += "c|"
    line += "}"
    print(line)
    print("\\hline")
 
    line = ""
    for p in proc:
        line += " & " + str(p)
    line += "\\\\\\hline"
    print(line)
 
    for model in inputs:
        baseline = keyFromFile(get_filename("relaxed-fair", model, 1, 0), "time")
        line = model[0] + " & {:.3f}s".format(1. * baseline / 1000)

        for p in proc:
            if p == 1:
                continue
            time = keyFromFile(get_filename("relaxed-fair", model, p, 0), "time")
            line += " & {:.2f}".format(1. * baseline / time)
        line += "\\\\\\hline"
        print(line)

def rebuttal_table():
    rebalgo = [("relaxed-fair", 0), ("relaxed-smart-splash-fair", 2), ("relaxed-smart-splash-fair", 5), ("randomr-splash-fair", 2), ("randomr-splash-fair", 5)]

    for model in inputs:
        print(model)
        line = "proc"
        for pairs in rebalgo:
            line += " & " + aliases[pairs[0]] + " H = " + str(pairs[1])
        line += "\\\\\\hline"
        print(line)

        for p in [1, 70]:
            line = "p = {}".format(p)
            for pairs in rebalgo:
                time = keyFromFile(get_filename(pairs[0], model, p, pairs[1]), "time")
                line += " & " + (str(time) if time != 0 else "INF")
            line += "\\\\\\hline"
            print(line)
        print()

def large_run_bars(key):
    prefix = "out/large_logs"

    procs = [20, 35, 70]
    algos = [("synchronous", 0),
             ("splash-fair", 10),
             ("relaxed-fair", 0)]
    
    labels = [str(x) for x in procs]

    x = np.arange(len(labels))
    width = 0.2
    fig, ax = plt.subplots()
    rects = []

    if key == "time":
        baseline = 1000
        ax.set_ylabel("Time (s)")
        ax.set_xlabel("Processes")
        ax.set_title("Running time")
    elif key == "updates":
        baseline = keyFromFile(get_filename(prefix, "residual", ("ising", 1000), 1, 0), key)
        ax.set_ylabel("Updates relative to residual BP")
        ax.set_xlabel("Processes")
        ax.set_title("Number of updates")

    for i in range(len(algos)):
        algo = algos[i]
        values = []

        for proc in procs:
            value = keyFromFile(get_filename(prefix, algo[0], ("ising", 1000), proc, algo[1]), key)
            values.append(value / baseline)

        r = ax.bar(x + i * width - (len(algos) - 1) * width / 2, values, width, label=algorithmName(algo[0], algo[1]))
        rects.append(r)

    ax.set_xticks(x)
    ax.set_xticklabels(labels)
#    ax.legend()

    def autolabel(rects):
        """Attach a text label above each bar in *rects*, displaying its height."""
        for rect in rects:
            height = rect.get_height()
            ax.annotate('{:.2f}'.format(height),
                        xy=(rect.get_x() + rect.get_width() / 2, height),
                        xytext=(0, 3),  # 3 points vertical offset
                        textcoords="offset points",
                        ha='center', va='bottom')

    for r in rects:
        autolabel(r)

    plotname = "out/plots/bars-{}.pdf".format(key)
    plt.savefig(plotname, bbox_inches='tight')

 
if sys.argv[1] == "run":
    compile()
    run(int(sys.argv[2]))
if sys.argv[1] == "plot":
    if len(sys.argv) == 2:
        plot("all")
    else:
        plot(sys.argv[2], sys.argv[3])
if sys.argv[1] == "relaxed_table":
    relaxed_table()
if sys.argv[1] == "against_nonrelaxed_table":
    against_nonrelaxed_table()
if sys.argv[1] == "speedup_table":
    speedup_table()
if sys.argv[1] == "rebuttal":
    rebuttal_table()
if sys.argv[1] == "large_run":
    if sys.argv[3] == "1":
        compile()
    large_run(int(sys.argv[2]), sys.argv[3] == "1")
if sys.argv[1] == "large_run_bars":
    large_run_bars(sys.argv[2])