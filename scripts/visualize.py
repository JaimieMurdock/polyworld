#!/usr/bin/python2
"""
visualize.py

Script to generate visualizations from the agent.visualization module. Operates
as a master main for the scripts in polyworld/scripts/agent/visualization/

Usage:

    ./visualize.py [MODE] [OPTIONS]

barplot         fingerprint graph (gene values and complexity by cluster)
population      population graph
complexity      plots TSE neural complexity
contacts        plots offspring per contact and offspring per contact ratio
distance        plots geographic distance at birth against genetic distance
genome          plots average gene value over time
movie           creates movie
scatter         creates a scatter plot of metrics against time (like plotgenome, but with data points)
"""

if __name__ == '__main__':
    from argparse import ArgumentParser
    parser = ArgumentParser(description="Visualizes Polyworld run data.",
        epilog="""
        Modes:

        population      produces a 
        """)


    parser.add_argument('mode', help='set the mode', 
        choices=['population'])
    parser.add_argument('--run', dest='run_dir', action='store', 
        default='../run/', help="set the run directory (default: '../run/')")
    parser.add_argument('-c', '--cluster', dest='cluster', action='store',
        help="set the cluster data file")

    args = parser.parse_args()
