import itertools
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os
import seaborn as sns
import subprocess
import sys

proc = [143] #[1, 2, 6, 10, 20, 30, 40, 50, 70]

# all
algorithms = ["concurrent-unfair", "concurrent-fair", "relaxed-unfair", "relaxed-fair", "splash-unfair", "splash-fair", "relaxed-splash-unfair", "relaxed-splash-fair", "relaxed-smart-splash-unfair", "relaxed-smart-splash-fair"]
# small
#algorithms = ["relaxed-unfair", "relaxed-fair", "splash-unfair", "splash-fair", "relaxed-splash-unfair", "relaxed-splash-fair", "relaxed-smart-splash-unfair", "relaxed-smart-splash-fair"]
# splash
#algorithms = ["splash-unfair", "splash-fair", "relaxed-splash-unfair", "relaxed-splash-fair", "relaxed-smart-splash-unfair", "relaxed-smart-splash-fair"]
# fair
#algorithms = ["concurrent-fair", "relaxed-fair", "splash-fair", "relaxed-splash-fair", "relaxed-smart-splash-fair"]

inputs = [("ising", 300), ("potts", 300)]

#splash_h = [5, 10]
#splash_h = [5]
splash_h = [2, 5, 10]

def compile():
    os.system("ant compile")

def run(T):
   if not os.path.isdir("out/logs"):
        os.makedirs("out/logs")
   for setting in itertools.product(proc, algorithms, inputs):
       hs = [0]
       if "splash" in setting[1]:
           hs = splash_h
       for h in hs:
          arguments = "{} {} {} {} {}".format(setting[1], setting[2][0], setting[2][1], setting[0], h)
          outfile = "out/logs/{}.txt".format(arguments.replace(" ", "-"))
          commandline = "java -cp bin bp.testing.Main {}".format(arguments)

          if os.path.isfile(outfile):
              os.remove(outfile)
          print(commandline)
          for i in range(T):
              subprocess.call("{} >> {}".format(commandline, outfile), shell="True")

def timeFromFile(filename):
    f = open(filename, 'r')
    total = 0
    number = 0
    for line in f.readlines():
        total += int(line.split(":")[1].strip())
        number += 1
    return total / number

def algorithmName(algorithm, h):
   if "splash" in algorithm:
       return "{} {}".format(algorithm, h)
   else:
       return algorithm

def plot():
   if not os.path.isdir("out/plots"):
       os.makedirs("out/plots")

#   sns.set_palette(sns.color_palette("husl", 12))
#   sns.set_palette(sns.color_palette("hls", 12))
   sns.set_palette(sns.color_palette("Paired"))

   for data in inputs:
       print(data)
       plt.clf()
       fig = plt.figure()
       ax = fig.add_subplot(111)
#       ax.set_prop_cycle('color', plt.cm.Spectral(np.linspace(0, 1, 20))) #)
#       cm = plt.get_cmap('gist_rainbow')
#       ax.set_color_cycle([cm(1.*i / len(inputs)) for i in range(len(algorithms))])

       for hid in range(len(splash_h)):
           for algorithm in algorithms:
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
                   arguments = "{}-{}-{}-{}-{}".format(algorithm, data[0], data[1], proc, h)
                   filename = "out/logs/{}.txt".format(arguments)
                   if not os.path.isfile(filename):
                       continue
                   x.append(proc)
                   y.append(timeFromFile(filename))
               ax.plot(x, y, label=algorithmName(algorithm, h))

#               arguments = "{}-{}-{}-{}-{}".format(algorithm, data[0], data[1], 143, h)
#               y = timeFromFile("out/logs/{}.txt".format(arguments))
#               print("{:30}: {} s".format(algorithmName(algorithm, h), y))
       plt.legend()
       plotname = "out/plots/{}-{}.png".format(data[0], data[1])
       plt.savefig(plotname)

# subprocess.call(command, stdout=f, shell=True, stderr=subprocess.STDOUT, env=my_env)

if sys.argv[1] == "run":
    compile()
    run(int(sys.argv[2]))
if sys.argv[1] == "plot":
    plot()