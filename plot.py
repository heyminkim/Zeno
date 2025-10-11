import argparse
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
import numpy as np
import sys
import math
import inspect
from collections import defaultdict
from mpl_toolkits.axes_grid1.inset_locator import inset_axes, mark_inset

# For this to work:
# https://stackoverflow.com/questions/42097053/matplotlib-cannot-find-basic-fonts
available_fonts = set(f.name for f in fm.fontManager.ttflist)
use_times = "Times New Roman" in available_fonts

if use_times:
    rc_fonts = {
        "font.family": "serif",
        "font.serif": ["Times New Roman"],
        "font.size": 9.5,
        "mathtext.fontset": "custom",
        "mathtext.rm": "Times New Roman",
        "mathtext.it": "Times New Roman:italic",
        "mathtext.bf": "Times New Roman:bold",
    }
else:
    rc_fonts = {
        "font.family": "serif",
        "font.size": 9.5,
    }
matplotlib.rcParams.update(rc_fonts)

#66c2a5
#fc8d62
#8da0cb
#e78ac3
#a6d854

# https://www.simplifiedsciencepublishing.com/resources/best-color-palettes-for-scientific-figures-and-data-visualizations
PLOT_STYLE_KWARGS = {
    "Zeno": {"marker": 'o', "color": '#e02b35', "zorder": 12, "label": "Zeno"},
    "ZenoV": {"marker": 'o', "color": '#e02b35', "zorder": 12, "label": "Zeno-V"},
    "ZenoR": {"marker": 'o', "color": '#e02b35', "zorder": 12, "label": "Zeno-R", "linestyle": ":"},
    "VZF": {"marker": 'o', "color": '#e02b35', "zorder": 12, "label": "Zeno"},
    "Aleph": {"marker": 's', "color": '#59a89c', "zorder": 12, "label": "Aleph"},
    # Styles for fractional expansion with IZF
    "IZF1_8": {"marker": 'o', "color": '#e02b35', "zorder": 12, "label": r"Zeno ($r$ = 1)"},
    "IZF2_8": {"marker": '.', "color": '#cb5411', "zorder": 9, "label": r"Zeno ($r$ = 2)", "linestyle": "dotted"},
    "IZF3_8": {"marker": '.', "color": '#b26e00', "zorder": 8, "label": r"Zeno ($r$ = 3)", "linestyle": "dashed"},
    "IZF4_8": {"marker": '.', "color": '#988100', "zorder": 7, "label": r"Zeno ($r$ = 4)", "linestyle": "dashdot"},
    "IZF1_7": {"marker": '.', "color": '#6f192d', "zorder": 11, "label": r"Zeno ($r$ = 1, $f$ = 7)"},
    # Styles for fractional expansion (exp 1 and 2)
    "IZF1_16": {"marker": 'o', "color": '#e02b35', "zorder": 12, "label": r"$f=16,\ r=1$"},
    "IZF2_16": {"marker": '.', "color": '#cb5411', "zorder": 9, "label": r"$f=16,\ r=2$", "linestyle": "dotted"},
    "IZF3_16": {"marker": '.', "color": '#b26e00', "zorder": 8, "label": r"$f=16,\ r=3$", "linestyle": "dashed"},
    "IZF4_16": {"marker": '.', "color": '#988100', "zorder": 7, "label": r"$f=16,\ r=4$", "linestyle": "dashdot"},
    "IZF1_15": {"marker": 'o', "color": '#6f192d', "zorder": 1, "label": r"$f=15,\ r=1$"},
    "IZF1": {"marker": 'o', "color": '#e02b35', "zorder": 12, "label": r"$r=1$"},
    "IZF2": {"marker": '.', "color": '#cb5411', "zorder": 9, "label": r"$r=2$", "linestyle": "dotted"},
    "IZF3": {"marker": '.', "color": '#b26e00', "zorder": 8, "label": r"$r=3$", "linestyle": "dashed"},
    "IZF4": {"marker": '.', "color": '#988100', "zorder": 7, "label": r"$r=4$", "linestyle": "dashdot"},
    # Styles for fractional expansion
    "Zeno1": {"marker": 'o', "color": '#e02b35', "zorder": 12, "label": r"Zeno ($r$ = 1)"},
    "Zeno2": {"marker": '.', "color": '#cb5411', "zorder": 9, "label": r"Zeno ($r$ = 2)", "linestyle": "dotted"},
    "Zeno3": {"marker": '.', "color": '#b26e00', "zorder": 8, "label": r"Zeno ($r$ = 3)", "linestyle": "dashed"},
    "Zeno4": {"marker": '.', "color": '#988100', "zorder": 7, "label": r"Zeno ($r$ = 4)", "linestyle": "dashdot"},
    "ZenoR1": {"marker": 'o', "color": '#e02b35', "zorder": 13, "label": r"ZenoR ($r$ = 1)"},
    "ZenoR2": {"marker": '.', "color": '#cb5411', "zorder": 9, "label": r"ZenoR ($r$ = 2)", "linestyle": "dotted"},
    "ZenoR3": {"marker": '.', "color": '#b26e00', "zorder": 8, "label": r"ZenoR ($r$ = 3)", "linestyle": "dashed"},
    "ZenoR4": {"marker": '.', "color": '#988100', "zorder": 7, "label": r"ZenoR ($r$ = 4)", "linestyle": "dashdot"},
    "VZF1": {"marker": 'o', "color": '#e02b35', "zorder": 14, "label": r"Zeno ($r$ = 1)"},
    "VZF2": {"marker": '.', "color": '#cb5411', "zorder": 9, "label": r"VZF ($r$ = 2)", "linestyle": "dotted"},
    "VZF3": {"marker": '.', "color": '#b26e00', "zorder": 8, "label": r"VZF ($r$ = 3)", "linestyle": "dashed"},
    "VZF4": {"marker": '.', "color": '#988100', "zorder": 7, "label": r"VZF ($r$ = 4)", "linestyle": "dashdot"},
    # "Zeno4": {"marker": '.', "color": '#b26e00', "zorder": 8, "label": r"Zeno ($r$ = 4)", "linestyle": "dashed"},
    "Zeno8": {"marker": '.', "color": '#988100', "zorder": 7, "label": r"Zeno ($r$ = 8)", "linestyle": "dashdot"},
    "Aleph16": {"marker": 's', "color": '#59a89c', "zorder": 11, "label": "Aleph"},
    "Aleph15": {"marker": 'p', "color": '#59a89c', "zorder": 6, "label": r"Aleph ($f$ = 15)", "linestyle": ":"},
    "Aleph12": {"marker": 's', "color": '#59a89c', "zorder": 11, "label": "Aleph"},
    "Aleph7": {"marker": 'p', "color": '#59a89c', "zorder": 6, "label": r"Aleph ($f$ = 15)", "linestyle": ":"},
    "RSQF16": {"marker": '^', "color": '#a559aa', "zorder": 5, "label": "RSQF"},
    "RSQF12": {"marker": '^', "color": '#a559aa', "zorder": 5, "label": "RSQF"},
    "RSQF": {"marker": '^', "color": '#a559aa', "zorder": 5, "label": "RSQF"},
    # Styles for in-place expansion
    "IZFie1": {"marker": 'o', "color": '#e02b35', "zorder": 12, "label": r"Zeno"},
    "VZFie1": {"marker": '^', "color": '#fc8d62', "zorder": 14, "label": r"Zeno-VM"},
    "Alephie16": {"marker": 's', "color": '#59a89c', "zorder": 11, "label": "Aleph"},
    # Styles for widening
    "ZenoRW": {"marker": 'x', "color": '#e02b35', "zorder": 11, "label": "Resizable Array", "linestyle": "dotted"},
    "ZenoVW": {"marker": 'o', "color": '#e02b35', "zorder": 12, "label": "Virtual Memory"},
    # Styles for concurrency
    "Threads0": {"color": '#e25759', "zorder": 1, "label": "Single Thread"},
    "Threads1": {"color": '#103e75', "zorder": 11, "label": "Background Expansion, t = 1"},
    "Threads2": {"color": '#f0c571', "zorder": 10, "label": "t = 2"},
    "Threads3": {"color": '#7e4794', "zorder": 9, "label": "t = 3"},
    "Threads4": {"color": '#59a89c', "zorder": 8, "label": "t = 4"},
    "Threads8": {"color": '#7e4794', "zorder": 7, "label": "t = 8"},
    "VZFCcon8": {"marker": 'o', "color": '#f7777e', "label": r"$R=2^8$", "linestyle": "dotted"},
    "VZFCcon10": {"marker": 'o', "color": '#eb4b54', "label": r"$R=2^{10}$", "linestyle": "dashed"},
    "VZFCcon12": {"marker": 'o', "color": '#e02b35', "label": r"$R=2^{12}$"},
    "VZFCcon14": {"marker": 'o', "color": '#bd151e', "label": r"$R=2^{14}$", "linestyle": "dashdot"},
    "VZFCcon16": {"marker": 'o', "color": '#8f030b', "label": r"$R=2^{16}$", "linestyle": "dotted"},
    "VZFCinsert": {"color": '#e02b35', "label": r"insert"},
    "VZFCquery": {"color": '#fc8d62', "label": r"query"},
    # Styles for competitors
    "Bamboo": {"marker": '*', "color": '#fc8d62', "zorder": 4, "label": "BBF"},
    "LDCF": {"marker": 'd', "color": '#8da0cb', "zorder": 3, "label": "LDCF"},
    "E2CF": {"marker": 'P', "color": '#e78ac3', "zorder": 2, "label": "E2CF"},
}

XLABELS = {
    "ie_size": r"execution time (s)",
    "exp_delete_size_size": r"execution time (s)",
    "bamboo_size": r"normalized runtime",
    "con_throughput": r"execution time (s)",
    # Labels for void entries and deletion
    "exp_void_delete": r"age of target entry",
    # Labels for concurrency
    "exp_concurrency": "# threads",
    "exp_concurrent_rw_insert": r"execution time ($s$)",
}

YLABELS = {
    # Labels for zeno microbenchmark (exp 1)
    "exp_insert_spaceamp_insert": r"avg. insert latency ($\mu$s)",
    "exp_insert_spaceamp_loadfactor": r"persistent space amp.",
    # Labels for zeno microbenchmark (exp 2)
    "exp_fpr_writeamp_fpr": r"false positive rate",
    "exp_fpr_writeamp_amp": r"execution time (s)",
    # Labels for void entries and deletion
    "exp_delete_size_delete": r"avg. delete latency ($\mu$s)",
    "exp_delete_size_size": r"filter size (MB)",
    "exp_void_size": r"size ratio (Zeno/Aleph)",
    "exp_void_delete": r"avg. delete latency ($\mu$s)",
    # Labels for fractional expansion
    "fe_size": r"filter size (MB)",
    "fe_insert": r"avg. insert latency ($\mu$s)",
    "fe_query": r"avg. query latency ($\mu$s)",
    "fe_fpr": "false positive rate",
    "fe_amp": "total execution time (s)",
    # Labels for in-place expansion
    "ie_size": r"filter size (MB)",
    "ie_insert": r"avg. insert latency ($\mu$s)",
    "ie_query": r"avg. query latency ($\mu$s)",
    "ie_fpr": "false positive rate",
    # Labels for competitors
    "bamboo_size": r"filter size (MB)",
    "bamboo_insert": r"avg. insert latency ($\mu$s)",
    "bamboo_pquery": r"avg. pos. query latency ($\mu$s)",
    "bamboo_nquery": r"avg. neg. query latency ($\mu$s)",
    "bamboo_fpr": "false positive rate",
    "widening_fpr_zeno": "false positive rate",
    "con_throughput": "million inserts / second",
    "con_overall": r"execution time ($s$)",
    "con_insert": r"avg. insert latency ($\mu$s)",
    "con_read": r"avg. query latency ($\mu$s)",
    # Labels for concurrency
    "exp_concurrency": r"million inserts / $s$",
    "exp_concurrent_rw_insert": r"million operations / $s$",
}

def running_average(values, window):
    if window <= 1 or window > len(values):
        return values
    return np.convolve(values, np.ones(window)/window, mode='valid')

def filter_kwargs(func, kwargs):
    sig = inspect.signature(func)
    return {k: v for k, v in kwargs.items() if k in sig.parameters}

LINES_STYLE = {
    "mew":0.5, 
    "markersize": 4.5, 
    "linewidth": 0.5, 
    "fillstyle": "none"
}

parser = argparse.ArgumentParser()
parser.add_argument('-s', '--savefilename', type=str, required=True)
parser.add_argument('-f', '--filename', type=str, required=False)
parser.add_argument('--filenames', nargs='+', help="List of file names to plot")
parser.add_argument('-x', '--xlabel', type=str)
parser.add_argument('-y', '--ylabel', type=str)
parser.add_argument('-l', '--log', action='store_true', help="Enable or disable log scale potting")
parser.add_argument('--log_y', action='store_true', help="Enable or disable log scale on y axis")
parser.add_argument('-i', '--interleave', action='store_true')
parser.add_argument('--no_legend', action='store_true')
parser.add_argument('--no_xlabel', action='store_true')
parser.add_argument('--normalize', action='store_true')
parser.add_argument('--png', action='store_true')
parser.add_argument('--wideplot', action='store_true')
parser.add_argument('--running_average', type=int, default=0, 
                    help="Window size for running average (0 disables it)")
parser.add_argument('--max_columns', type=int, default=4, 
                    help="Maximum number of columns")
parser.add_argument('--legend_column', type=int, default=0, 
                    help="Column of the axis for the legend")
parser.add_argument('--letter', type=int, default=0, help="Embed alphabet letters in x axis")
# action='store_true', help="Embed alphabet letters in x axis")
args = parser.parse_args()

savefilename = args.savefilename
filenames = args.filenames
wideplot = args.wideplot

max_cols = args.max_columns
n_files = len(filenames)
n_rows = math.ceil(n_files / max_cols)

dpi = 200
num_cols = min(n_files, max_cols)
width_px = 450 * num_cols
height_px = 350 #* 1.1 * n_rows
if wideplot:
    width_px = 2 * width_px
    height_px = 330
# if num_cols == 4:
#     height_px = 500
plt.figure(figsize=(width_px / dpi, height_px / dpi), dpi=dpi)

fontsize=12
labelsize=12
ticksize=8
legendfontsize=12
if num_cols <= 2:
    fontsize=10
    labelsize=10
    ticksize=8
    legendfontsize=9
    if wideplot:
        legendfontsize=8

# Number of columns in the legend, reactive to the number of columns in plot
legend_ncols = 7 if num_cols > 2 else 3
if wideplot:
    legend_ncols = 7

# Configure the legend column of the axis
legend_column = args.legend_column

fig, axes = plt.subplots(n_rows, num_cols, figsize=(width_px / dpi, height_px / dpi), dpi=dpi)

if len(filenames) == 1:
    axes = [axes]

if num_cols != 1:
    axes = axes.flatten()

for idx, (ax, filename) in enumerate(zip(axes, filenames)):
    file_path = "./results/" + filename + ".csv"
    file = open(file_path, 'r')

    datas = []
    length = 0
    prev_label = None
    sum_values = defaultdict(list)
    maxlength = 0
    minlength = float('inf')
    count = 0

    letter = ""
    if args.letter == 0:
        letter = chr(ord('a') + idx)
        letter = rf"$\mathbf{{({letter})}}$ "
    elif args.letter < 0:
        letter = ""
    else:
        letter = chr(ord('a') + args.letter - 1)
        letter = rf"$\mathbf{{({letter})}}$ "


    for spine in ax.spines.values():
        spine.set_zorder(20)

    if filename == 'con_overall':
        x_values = []
        y_values = []
        base_values = []

        y_value_groups = defaultdict(list)
        base_value_groups = defaultdict(list)

        for line in file:
            p = line.strip().rstrip(',').split(',')
            label = p[0]
            y_val = float(p[1])
            expansion_time = float(p[2])
            y_value_groups[label].append(y_val)
            base_value_groups[label].append(expansion_time)
            if label not in x_values:
                x_values.append(label)
        
        positions = list(range(len(x_values)))
        
        y_values = [np.mean(y_value_groups[label]) for label in x_values]
        base_values = [np.mean(base_value_groups[label]) for label in x_values]

        for i, label in enumerate(x_values):
            base_kwargs = PLOT_STYLE_KWARGS[label].copy()
            base_kwargs['alpha'] = 0.3
            base_kwargs['color'] = 'black'
            ax.bar(positions[i], y_values[i], **PLOT_STYLE_KWARGS[label])
            ax.bar(positions[i], base_values[i], **base_kwargs)

        ax.set_xlabel("# threads", fontsize=9)
        ax.set_ylabel(YLABELS[filename], fontsize=9, labelpad=1)

        # positions = list(range(len(x_values)))
        ax.tick_params(axis='x', labelsize=8)
        ax.set_xticks(positions)
        ax.set_xticklabels([f"t={label[-1]}" for label in x_values])
    # Box plot for write amplification
    elif filename == "fe_amp" or filename == "exp_fpr_writeamp_amp":
        x_values = []
        y_values = []
        base_values = []

        y_value_groups = defaultdict(list)

        for line in file:
            p = line.strip().rstrip(',').split(',')
            label = p[0]
            # if any(sub in label for sub in ["Aleph", "RSQF"]):
            #     continue
            y_val = float(p[1])
            y_value_groups[label].append(y_val)
            if label not in x_values:
                x_values.append(label)
        
        y_values = [np.mean(y_value_groups[label]) for label in x_values]
        
        for i, label in enumerate(x_values):
            style = PLOT_STYLE_KWARGS[label]
            color = style["color"]
            # ax.bar(label, y_values[i], **PLOT_STYLE_KWARGS[label])
            ax.bar(label, y_values[i], color=color)

        ax.set_xlabel(letter + r'expansion ratio ($r$)', fontsize=fontsize)
        ax.set_ylabel(YLABELS[filename], fontsize=fontsize, labelpad=1)
        ax.set_xticks(x_values)
        ax.set_xticklabels([s.split("_", 1)[0][-1] for s in x_values])
        ax.spines[['right', 'top']].set_visible(False)
        ax.yaxis.set_tick_params(pad=0)
        ax.tick_params(axis='both', which='major', labelsize=ticksize)
        ax.tick_params(axis='both', which='minor', labelsize=ticksize)
        file.close()
    elif filename in ["bamboo_size"]:   # Normalized time
        visited = set()
        for line in file:
            p = line.strip().rstrip(',').split(',')
            label = p[0]
            if label in visited:
                continue
            visited.add(label)
            data_str = p[1:]
            data = [max(0.1,float(data_str[i])-7) for i in range(len(data_str))]
            data = [data[i] - data[0] for i in range(len(data_str))]
            # x = [i * 0.1 for i in range(len(data))]
            x = [i / (len(data) - 1) for i in range(len(data))]
            line, = ax.plot(x, data, **PLOT_STYLE_KWARGS[label], **LINES_STYLE)
            line.set_marker("")
        ax.set_xlabel(letter + XLABELS[filename], fontsize=fontsize)
        ax.set_ylabel(YLABELS[filename], fontsize=fontsize, labelpad=1)
            # ax.set_ylim(top=25)
        if filename == "bamboo_size":
            ax.set_yscale('log')
        else:
            ax.set_yscale('linear')
        ax.spines[['right', 'top']].set_visible(False)
        ax.yaxis.set_tick_params(pad=0)
        file.close()
    elif filename in ["ie_size"]:   # Not normalized time
        visited = set()
        for line in file:
            p = line.strip().rstrip(',').split(',')
            label = p[0]
            if label in visited:
                continue
            visited.add(label)
            data_str = p[1:]
            data = [max(0.1,float(data_str[i])-7) for i in range(len(data_str))]
            data = [data[i] - data[0] for i in range(len(data_str))]
            x = [i * 0.1 for i in range(len(data))]
            # line, = ax.plot(x, data, **PLOT_STYLE_KWARGS[label], **LINES_STYLE)
            line, = ax.plot(x, data, **PLOT_STYLE_KWARGS[label], **{**LINES_STYLE, "linewidth": 0.8 })
            line.set_marker("")
        ax.set_xlabel(letter + XLABELS[filename], fontsize=fontsize)
        ax.set_ylabel(YLABELS[filename], fontsize=fontsize, labelpad=1)
        if filename == "bamboo_size":
            ax.set_yscale('log')
        else:
            ax.set_yscale('linear')
        ax.spines[['right', 'top']].set_visible(False)
        ax.yaxis.set_tick_params(pad=0)
        file.close()
    elif filename == "exp_void_size":
        for line in file:
            p = line.strip().rstrip(',').split(',')
            label = p[0]
            data_str = p[1:]

            # Parse into (x, y) pairs
            data = [(float(data_str[i]), float(data_str[i + 1])) for i in range(0, len(data_str), 2)]

            # Aggregate y-values for duplicate x's in the same line
            line_data = defaultdict(list)
            for x, y in data:
                line_data[x].append(y)
            
            # Average y-values for same x within this line
            averaged_line_data = {x: sum(ys) / len(ys) for x, ys in line_data.items()}
            
            length = len(averaged_line_data)
            maxlength = max(maxlength, length)
            minlength = min(minlength, length)

            if label == prev_label or prev_label is None:
                for x, y in averaged_line_data.items():
                    sum_values[x].append(y)
                count += 1
            else:
                # Average across all lines for previous label
                avg_values = {x: sum(ys) / len(ys) for x, ys in sum_values.items()}
                # Flatten to list sorted by x
                sorted_avg = []
                for x in sorted(avg_values.keys()):
                    sorted_avg.extend([x, avg_values[x]])
                datas.append({'label': prev_label, 'data': sorted_avg})

                # Reset for new label
                sum_values = defaultdict(list)
                for x, y in averaged_line_data.items():
                    sum_values[x].append(y)
                count = 1

            prev_label = label

        if count > 0:
            avg_values = {x: sum(ys) / len(ys) for x, ys in sum_values.items()}
            sorted_avg = []
            for x in sorted(avg_values.keys()):
                sorted_avg.extend([x, avg_values[x]])
            datas.append({'label': prev_label, 'data': sorted_avg})

        x = np.arange(1, minlength)
        nd = len(datas)

        x1 = datas[0]['data'][0::2]
        y1 = datas[0]['data'][1::2]
        x2 = datas[1]['data'][0::2]
        y2 = datas[1]['data'][1::2]

        ratios = [a / b if b != 0 else float('nan') for b, a, in zip(y1, y2)]
        ax.plot(x1, ratios, marker='o', linestyle='-', color='#e02b35', label='size ratio', **LINES_STYLE)

        ax.set_xlabel(letter + XLABELS.get(filename, "# entries"), fontsize=fontsize)
        ax.set_ylabel(YLABELS[filename], fontsize=fontsize, labelpad=1)
        ax.set_xscale('log')

        ax.autoscale_view()
        ax.tick_params(axis='both', which='major', labelsize=ticksize)
        ax.tick_params(axis='both', which='minor', labelsize=ticksize)
        ymin, ymax = ax.get_ylim()
        ax.set_ylim(ymin, ymax + 0.05 * (ymax - ymin))
        ax.spines[['right', 'top']].set_visible(False)
        ax.yaxis.set_tick_params(pad=1)
        file.close()
    else:
        for line in file:
            p = line.strip().rstrip(',').split(',')
            label = p[0]
            data_str = p[1:]

            # Parse into (x, y) pairs
            data = [(float(data_str[i]), float(data_str[i + 1])) for i in range(0, len(data_str), 2)]

            # Aggregate y-values for duplicate x's in the same line
            line_data = defaultdict(list)
            for x, y in data:
                line_data[x].append(y)
            
            # Average y-values for same x within this line
            averaged_line_data = {x: sum(ys) / len(ys) for x, ys in line_data.items()}
            
            length = len(averaged_line_data)
            maxlength = max(maxlength, length)
            minlength = min(minlength, length)

            if label == prev_label or prev_label is None:
                for x, y in averaged_line_data.items():
                    sum_values[x].append(y)
                count += 1
            else:
                # Average across all lines for previous label
                avg_values = {x: sum(ys) / len(ys) for x, ys in sum_values.items()}
                # Flatten to list sorted by x
                sorted_avg = []
                for x in sorted(avg_values.keys()):
                    sorted_avg.extend([x, avg_values[x]])
                datas.append({'label': prev_label, 'data': sorted_avg})

                # Reset for new label
                sum_values = defaultdict(list)
                for x, y in averaged_line_data.items():
                    sum_values[x].append(y)
                count = 1

            prev_label = label

        if count > 0:
            avg_values = {x: sum(ys) / len(ys) for x, ys in sum_values.items()}
            sorted_avg = []
            for x in sorted(avg_values.keys()):
                sorted_avg.extend([x, avg_values[x]])
            datas.append({'label': prev_label, 'data': sorted_avg})

        x = np.arange(1, minlength)
        nd = len(datas)

        # Set inset axis for filter size graph
        if filename == "fe_size":
            axins = inset_axes(ax, width="50%", height="50%", loc="upper left")
            axins.set_xscale('log')
            axins.tick_params(axis='x', which='both', bottom=False, labelbottom=False)
            axins.tick_params(axis='y', which='both', left=False, labelleft=False)
            axins.set_xlim(5010945, 17421789)
            axins.set_ylim(15, 45)
            connectors = mark_inset(ax, axins, loc1=1, loc2=3)
            smallbox, con1, con2 = connectors
            smallbox.set_linewidth(0.8)
            smallbox.set_zorder(15)
            for con in (con1, con2):
                con.set_linestyle("--")
                con.set_linewidth(0.8)

        for i, data in enumerate(datas):
            x_values = []
            y_values = []
            x_values = data['data'][0::2]
            y_values = data['data'][1::2]

            if args.running_average == 0 or len(x_values) < args.running_average:
                if filename == "fe_size":
                    ax.step(x_values, y_values, **PLOT_STYLE_KWARGS[data['label']], **LINES_STYLE,
                            where='post')
                    axins.step(x_values, y_values, **PLOT_STYLE_KWARGS[data['label']], **LINES_STYLE,
                            where='post')
                else:
                    no_marker_array = ["exp_insert_spaceamp_loadfactor"]
                    ax.plot(x_values, y_values, **PLOT_STYLE_KWARGS[data['label']], **{**LINES_STYLE,
                    "markersize": 0 if filename in no_marker_array else LINES_STYLE["markersize"]})
                    if data['label'] == 'Threads0':
                        ax.fill_between(x_values, y_values, **PLOT_STYLE_KWARGS[data['label']])
            else:
                smoothed_y = running_average(y_values, args.running_average)
                smoothed_x = x_values[:len(smoothed_y)]
                ax.plot(smoothed_x, smoothed_y, **PLOT_STYLE_KWARGS[data['label']], **{**LINES_STYLE,
                        "linewidth": 0.8 if "concurrent" in filename else LINES_STYLE["linewidth"]})
                if data['label'] == 'Threads0':
                    ax.fill_between(x_values, y_values, color='#e25759')

        ax.set_xlabel(letter + XLABELS.get(filename, "# entries"), fontsize=fontsize)
        ax.set_ylabel(YLABELS[filename], fontsize=fontsize, labelpad=1)

        # Change x axis to linear scale
        linear_filenames = ["con", "ie_size", "exp_void_delete", "exp_void_size"]
        if any(sub in filename.lower() for sub in linear_filenames):
            ax.set_xscale('linear')
        else:
            ax.set_xscale('log')

        # Change y axis to log scale if it is on fpr
        log_filenames = ["exp_delete_size_size", "fpr", "exp_concurrency"]
        # log_filenames = []
        if any(sub in filename.lower() for sub in log_filenames):
            ax.set_yscale('log')
        else:
            ax.set_yscale('linear')
            ax.set_ylim(bottom=0)
        
        if filename == "fe_insert":
            ax.set_ylim(bottom=0)
        
        # if filename == "bamboo_insert":
        #     ax.set_ylim(top=2)

        ax.autoscale_view()
        ax.tick_params(axis='both', which='major', labelsize=ticksize)
        ax.tick_params(axis='both', which='minor', labelsize=ticksize)
        ymin, ymax = ax.get_ylim()
        ax.set_ylim(ymin, ymax + 0.05 * (ymax - ymin))
        ax.spines[['right', 'top']].set_visible(False)
        ax.yaxis.set_tick_params(pad=1)
        file.close()

fig_height_in = height_px / dpi
offset_px = 45
if num_cols == 4:
    offset_px = 80
elif wideplot:
    offset_px = 30

# Special case for concurrency
if filename == "exp_concurrency":
    fontsize=8

h_l = axes[legend_column].get_legend_handles_labels()
# When num_cols == 2 but the legend must line break. Assumes no more than two lines will happen
if len(h_l[1]) > legend_ncols:
    offset_px = 80
    columnspacing=0.1

    # Reorder handles and labels to be row oriented
    # https://stackoverflow.com/questions/66783109/matplotlibs-legend-how-to-order-entries-by-row-first-rather-than-by-column
    reorder=lambda hl,nc:(sum((lis[i::nc]for i in range(nc)),[])for lis in hl)

    offset_norm = offset_px / (fig_height_in * dpi) 
    fig.legend(*reorder(h_l, legend_ncols),
            loc='upper center', 
            bbox_to_anchor=(0.5, 1 + offset_norm), 
            ncol=legend_ncols,
            frameon=False,
            fontsize=legendfontsize,
            columnspacing=0.8
    )
else:
    offset_norm = offset_px / (fig_height_in * dpi) 
    fig.legend(h_l[0],
            h_l[1],
            loc='upper center', 
            bbox_to_anchor=(0.5, 1 + offset_norm), 
            ncol=legend_ncols,
            frameon=False,
            fontsize=legendfontsize,
            columnspacing=0.8
    )

plt.tight_layout()

plt.subplots_adjust(wspace=0.28)
if n_rows > 1:
    plt.subplots_adjust(hspace=0.4)

if args.png:
    save_name = "./results/" + savefilename + '.png'
else:
    save_name = "./results/" + savefilename + '.pdf'
plt.savefig(save_name, bbox_inches='tight', pad_inches=0)