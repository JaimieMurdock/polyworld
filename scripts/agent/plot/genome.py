#!/usr/bin/python
import matplotlib
import pylab 

params = {'backend': 'pdf',
          'grid.linewidth' : 0.5,
          'lines.linewidth' : 0.5,
          'axes.linewidth' : 0.5,
          'patch.linewidth' : 0.25,
          'xtick.major.pad' : 2,
          'xtick.major.size' : 2,
          'ytick.major.pad' : 2,
          'ytick.major.size' : 2,
          'axes.titlesize': 6,
          'axes.labelsize': 5,
          'text.fontsize': 5,
          'legend.fontsize': 4,
          'xtick.labelsize': 4,
          'ytick.labelsize': 4,
          'text.usetex': False,
          'figure.figsize': (3.4,1.7),
          'figure.subplot.bottom' : 0.125
          }
pylab.rcParams.update(params)

from agent import *
import numpy as np
from math import sqrt
from agent.genome import get_label

def plot(genes, filename="plot.png", func=lambda a: a.id, 
         plot_title='', cmap='Paired', filter=lambda a: True, 
         draw_legend=False):
    p = get_population()
    pops = [[] for i in xrange(30000)]

    cmap = pylab.cm.jet

    for a in p:
        for step in range(a.birth, a.death):
            pops[step].append(a)

    lines=[]
    for i, gene in enumerate(genes):
        color = cmap(float(i)/len(genes))

        data = []
        err_up = []
        err_down = []
        for step in xrange(30000):
            datum = [agent.genome[gene] for agent in pops[step]]
            mean = sum(datum) / len(pops[step])
            stderr = np.std(datum) / sqrt(len(pops[step]))

            #err_up.append(min(mean+stderr, 255))
            #err_down.append(max(mean-stderr,0))
            err_up.append(min(mean+np.std(datum), 255))
            err_down.append(max(mean-np.std(datum),0))
            data.append(mean)
        fill = pylab.fill_between(range(0,30000,5), err_up[::5], err_down[::5], 
                                  color=color, alpha=0.35, linewidth=0,
                                  edgecolor=None, facecolor=color)
        #pylab.plot(range(30000), err_up, alpha=0.35, color=color)
        #pylab.plot(range(30000), err_down, alpha=0.35, color=color)
        line = pylab.plot(range(30000), data, color=color)
        lines.append(line)

    pylab.title("Size and Internal Neural Group Count (INGC)", weight='black')
    pylab.ylim(0, 255)
    pylab.xlim(0, 30000)
    pylab.ylabel("Raw Gene Value", weight='bold')
    pylab.xlabel("Time", weight='bold')
    pylab.legend(lines, [get_label(gene) for gene in genes], loc='center right')
    pylab.figtext(0,.948, '(d)', size=6, weight='black')
    pylab.savefig("plots/test.pdf", dpi=300)

plot([5,11])
