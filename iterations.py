import itertools
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os
import seaborn as sns
import subprocess
import sys

proc = [1, 2, 6, 10, 20, 30, 40, 50, 60, 70]

algorithms = [
#              ("residual", [1]),
              ("relaxed-fair", proc)
             ]
inputs = [("ising", 300), ("potts", 300)]

def compile():
    os.system("ant compile")

def run(T):
   if not os.path.isdir("out/logs-iterations"):
        os.makedirs("out/logs-iterations")
   for algorithm in algorithms:
       for proc in algorithm[1]:
           for data in inputs:
               arguments = "{} {} {} {} {}".format(algorithm[0], data[0], data[1], proc, 1)
               outfile = "out/logs-iterations/{}.txt".format(arguments.replace(" ", "-"))
               commandline = "java -XX:+UseRTMLocking -cp bin bp.testing.Main {}".format(arguments)
               if os.path.isfile(outfile):
                   os.remove(outfile)
               print(commandline)
               for i in range(T):
                    subprocess.call("{} >> {}".format(commandline, outfile), shell="True")

def iterationsFromFile(filename):
    f = open(filename, 'r')
    total = 0
    number = 0
    for line in f.readlines():
        if "Iterations" in line:
            total += int(line.split(":")[1].strip())
            number += 1
    return total / number

def plot():
    if not os.path.isdir("out/plots"):
        os.makedirs("out/plots")
    for data in inputs:
        print(data)
        plt.clf()
        fig = plt.figure()
        ax = fig.add_subplot(111)
        arguments = "{}-{}-{}-{}-{}".format("residual", data[0], data[1], 1, 1)
        outfile = "out/logs-iterations/{}.txt".format(arguments)
        basic_it = iterationsFromFile(outfile)

        for algorithm in algorithms:
            algo = algorithm[0]
            it = []
            for proc in algorithm[1]:
                arguments = "{}-{}-{}-{}-{}".format(algo, data[0], data[1], proc, 1)
                outfile = "out/logs-iterations/{}.txt".format(arguments)
                it.append(1. * (iterationsFromFile(outfile) - basic_it) / basic_it)

            ax.plot(algorithm[1], it, label=algo)
 
        plt.legend()
        plotname = "plots/iterations-{}-{}.png".format(data[0], data[1])
        plt.savefig(plotname)

if sys.argv[1] == "run":
    compile()
    run(int(sys.argv[2]))
if sys.argv[1] == "plot":
    plot()