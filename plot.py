import argparse
import math
from collections import Counter, defaultdict
from fractions import Fraction

import matplotlib
import matplotlib.font_manager as fm
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FormatStrFormatter, FuncFormatter, NullLocator
from mpl_toolkits.axes_grid1.inset_locator import inset_axes, mark_inset

RESULTS_DIR = "./results/"

# tuning knobs

DPI = 200

# Figure geometry in pixels. Pairs are (single-column, double-column) values.
COLUMN_WIDTH        = 500
ROW_HEIGHT          = (480, 370)
NO_LEGEND_HEIGHT    = (450, 380)
FOUR_COLUMN_HEIGHT  = 400               # double-column layout only
WIDE_PLOT_HEIGHT    = 330               # wideplot also doubles the total width
NARROW_PLOT_SIZE    = (900, 380)
WIDTH_STRETCH       = 1.05              # applied when the figure has 2 or 4 columns

# Font sizes. Pairs are (single-column, double-column) values.
FONT_SIZE               = (14, 11)
TICK_SIZE               = (12, 9)
LEGEND_FONT_SIZE        = (14, 11)
SMALL_LEGEND_FONT_SIZE  = (12, 11)
NARROW_FONT_SIZE        = 11
NARROW_TICK_SIZE        = 7
WIDE_LEGEND_FONT_SIZE   = 8

# Legend geometry: vertical gap between the figure and a shared top legend, in pixels.
LEGEND_OFFSET_PX            = 70
LEGEND_OFFSET_WIDE_PX       = 35    # when wideplot or an explicit --legend_ncols is given
LEGEND_OFFSET_WRAPPED_PX    = 80    # when the legend wraps onto a second row
MAX_LEGEND_NCOLS            = 8     # maximum legend entries per row

HSPACE = 0.4                    # vertical gap between subplot rows

SINGLE_LINES_STYLE = {"mew": 0.8, "markersize": 4.5, "linewidth": 0.8, "fillstyle": "none"}
DOUBLE_LINES_STYLE = {"mew": 0.5, "markersize": 4.5, "linewidth": 0.5, "fillstyle": "none"}

# Per-experiment knobs.
BAR_PLOT_FILES          = ["fe_amp", "exp_fpr_writeamp_amp", "exp_insert_spaceamp_maxamp"]
BAR_PLOT_TOTAL_INSERTS  = 2**28
SIZE_OVER_TIME_FILES    = ["ie_size", "exp_overall_size", "exp_widening_size"]
RATIO_FILES             = ["exp_contract_ratio", "exp_expand_ratio"]
MARKER_STRIDE           = 130
MARKER_STRIDE_OVERALL   = 90
NO_MARKER_FILES         = ["exp_insert_spaceamp_loadfactor"]
LINEAR_X_SUBSTRINGS     = ["con", "ie_size", "exp_void_delete", "exp_void_size",
                           "exp_contract_size", "exp_expand_size", "abs_inserts"]
LOG_Y_SUBSTRINGS        = ["exp_delete_size_size", "fpr", "exp_directory_size",
                           "exp_overall_insert", "exp_overall_nquery"]
THREAD_COUNT_XTICKS     = [1, 2, 4, 8, 16, 32, 64]
FRACTION_XTICKS         = [1 / 64, 1 / 32, 1 / 16, 1 / 8, 1 / 4, 1 / 2, 1]
FE_SIZE_INSET_XLIM      = (5010945, 17421789)
FE_SIZE_INSET_YLIM      = (15, 45)

# style tables

# https://stackoverflow.com/questions/42097053/matplotlib-cannot-find-basic-fonts
available_fonts = set(f.name for f in fm.fontManager.ttflist)
if "Times New Roman" in available_fonts:
    matplotlib.rcParams.update({
        "font.family": "serif",
        "font.serif": ["Times New Roman"],
        "font.size": 9.5,
        "mathtext.fontset": "custom",
        "mathtext.rm": "Times New Roman",
        "mathtext.it": "Times New Roman:italic",
        "mathtext.bf": "Times New Roman:bold",
    })
else:
    matplotlib.rcParams.update({
        "font.family": "serif",
        "font.size": 9.5,
    })

PLOT_STYLE_KWARGS = {
    # Fractional expansion (exp 1)
    "IZF1_16": {"marker": 'o', "color": '#e02b35', "zorder": 12, "label": r"$r=1$"},
    "IZF2_16": {"marker": '.', "color": '#cb5411', "zorder": 9, "label": r"$r=2$", "linestyle": "dotted"},
    "IZF3_16": {"marker": '.', "color": '#b26e00', "zorder": 8, "label": r"$r=3$", "linestyle": "dashed"},
    "IZF4_16": {"marker": '.', "color": '#988100', "zorder": 7, "label": r"$r=4$", "linestyle": "dashdot"},
    # In-place expansion (exp 2)
    "IZFie1": {"marker": 'o', "color": '#e02b35', "zorder": 16, "label": r"Zeno Filter"},
    "VZFie1": {"marker": 'o', "color": '#6f192d', "zorder": 14, "label": r"Zeno Filter-VM"},
    "Alephie16": {"marker": 's', "color": '#59a89c', "zorder": 11, "label": "Aleph Filter"},
    "Alephie10": {"marker": '.', "color": '#59a89c', "zorder": 8, "label": r"Aleph Filter ($f=10$)", "linestyle": "dotted"},
    # Widening (exp 3)
    "VZFnoW": {"marker": '.', "color": '#e02b35', "zorder": 18, "label": "Zeno Filter", "linestyle": "dotted"},
    "VZFW": {"marker": 'o', "color": '#e02b35', "zorder": 20, "label": "Zeno Filter (Widening)"},
    "AlephW": {"marker": 's', "color": '#59a89c', "zorder": 10, "label": "Aleph Filter (Widening)"},
    # Fixed-size blocks (exp 4)
    "FD": {"marker": '.', "color": '#fc8d62', "zorder": 8, "label": "Fixed-size Block", "linestyle": "dotted"},
    # Competitors (exp 7)
    "IZF": {"marker": 'o', "color": '#e02b35', "zorder": 20, "label": "Zeno Filter"},
    "IZF2": {"marker": '.', "color": '#cb5411', "zorder": 9, "label": r"Zeno Filter ($r=2)$", "linestyle": "dotted"},
    "VZF": {"marker": 'o', "color": '#6f192d', "zorder": 12, "label": "Zeno Filter-VM"},
    "Aleph": {"marker": 's', "color": '#59a89c', "zorder": 10, "label": "Aleph Filter"},
    "Bamboo": {"marker": 'x', "color": '#e78ac3', "zorder": 4, "label": "BBF"},
    "LDCF": {"marker": '+', "color": '#a559aa', "zorder": 3, "label": "LDCF"},
    "InfiniFilter": {"marker": 'D', "color": '#8da0cb', "zorder": 5, "label": "InfiniFilter"},
    # Concurrency (expansion threshold)
    "VZFcon7": {"marker": 'o', "color": '#f7777e', "label": r"$\alpha = 0.7$", "linestyle": "dotted"},
    "VZFcon4": {"marker": 'o', "color": '#e02b35', "label": r"$Zeno2$"},
    "VZFcon9": {"marker": 'o', "color": '#8f030b', "label": r"$\alpha = 0.9$", "linestyle": "dotted"},
    "RSQFcon4": {"marker": '^', "color": '#89b345', "label": r"$RSQF2$"},
    "VZFcon0": {"marker": 'o', "color": '#e02b35', "label": r"Zeno Filter"},
    "RSQFcon0": {"marker": '^', "color": '#89b345', "label": r"RSQF"},
    "VZFcon2": {"marker": 'o', "color": '#e02b35', "label": r"Zeno Filter"},
    "RSQFcon2": {"marker": '^', "color": '#89b345', "label": r"RSQF"},
    "RSQFcon4096": {"marker": '^', "color": '#89b345', "label": r"RSQF"},
    "VZFcon4096": {"marker": 'o', "color": '#e02b35', "label": r"Zeno Filter"},
    "VZFcon65536": {"marker": '^', "color": '#89b345', "label": r"RSQF"},
    # Concurrency (reads + writes)
    "VZFinsert": {"color": '#e02b35', "zorder": 12, "label": r"Zeno Filter"},
    "VZFquery": {"color": '#e02b35', "zorder": 11, "label": r"Zeno Filter"},
    "RSQFinsert": {"color": '#89b345', "zorder": 10, "label": r"RSQF"},
    "RSQFquery": {"color": '#89b345', "zorder": 10, "label": r"RSQF"},
    "RSQF": {"marker": '^', "color": '#89b345', "zorder": 51, "label": "RSQF"},
    # WiredTiger
    "NOFILTER": {"marker": 'o', "color": "#000000", "label": "Baseline"},
    "Buffer": {"color": "#000000"},
}

XLABELS = {
    "ie_size": r"execution time (s)",
    "exp_overall_size": r"execution time (s)",
    "exp_widening_size": "execution time (s)",
    "exp_contract_ratio": r"# contraction",
    "exp_expand_ratio": r"# expansion",
    "exp_concurrency": "# threads",
    "exp_concurrent_rw_insert": r"execution time ($s$)",
    "exp_concurrent_rw_query": r"execution time ($s$)",
    "exp_concurrency_one": "# threads",
    "exp_wiredtiger_size": r"execution time (s)",
    "exp_wiredtiger_nquery": r"dataset fraction",
    "exp_wiredtiger_pquery": r"dataset fraction",
    "exp_wiredtiger_insert": r"dataset fraction",
}

YLABELS = {
    "exp_insert_spaceamp_loadfactor": r"persistent space amp.",
    "fe_query": r"avg. query latency ($\mu$s)",
    "fe_fpr": "false positive rate",
    "fe_amp": r"avg. insert latency ($\mu$s)",
    "ie_size": r"filter size (MB)",
    "ie_insert": r"avg. insert latency ($\mu$s)",
    "ie_query": r"avg. query latency ($\mu$s)",
    "ie_fpr": "false positive rate",
    "exp_widening_fpr": "false positive rate",
    "exp_widening_size": "filter size (MB)",
    "exp_directory_size": "directory size (MB)",
    "exp_directory_query": r"avg. query latency ($\mu$s)",
    "exp_expand_ratio": "size ratio\n(Zeno/Aleph)",
    "exp_contract_ratio": r"size ratio (Zeno/Aleph)",
    "exp_overall_size": r"filter size (MB)",
    "exp_overall_insert": r"avg. insert latency ($\mu$s)",
    "exp_overall_nquery": r"avg. query latency ($\mu$s)",
    "exp_overall_fpr": "false positive rate",
    "exp_concurrency": r"million inserts / $s$",
    "exp_concurrency_one": r"million inserts / $s$",
    "exp_concurrent_rw_insert": r"million inserts / $s$",
    "exp_concurrent_rw_query": r"million queries / $s$",
    "exp_wiredtiger_nquery": r"avg. query latency ($\mu$s)",
    "exp_wiredtiger_pquery": r"avg. query latency ($\mu$s)",
    "exp_wiredtiger_insert": r"avg. insert latency ($\mu$s)",
    "exp_wiredtiger_size": r"filter size (MB)",
    "abs_inserts": r"avg. insert latency ($\mu$s)",
    "vqf": r"avg. insert latency ($\mu$s)",
}

# helper functions

def choose(single_column, single_value, double_value=None):
    return single_value if (single_column or double_value is None) else double_value


def running_average(values, window):
    if window <= 1 or window > len(values):
        return values
    return np.convolve(values, np.ones(window) / window, mode='valid')


def parse_line(line):
    parts = line.strip().rstrip(',').split(',')
    return parts[0], parts[1:]


def read_averaged_series(file, skip_labels=()):
    """Reads `label,x,y,x,y,...` lines into a list of (label, xs, ys) with xs sorted.
    Duplicate x values within a line are averaged, and so are consecutive lines that
    share the same label."""
    series = []
    prev_label = None
    grouped = defaultdict(list)

    def flush():
        averaged = {x: sum(ys) / len(ys) for x, ys in grouped.items()}
        xs = sorted(averaged.keys())
        series.append((prev_label, xs, [averaged[x] for x in xs]))

    for line in file:
        label, data_str = parse_line(line)
        if label in skip_labels:
            continue
        pairs = [(float(data_str[i]), float(data_str[i + 1])) for i in range(0, len(data_str), 2)]
        per_x = defaultdict(list)
        for x, y in pairs:
            per_x[x].append(y)

        if prev_label is not None and label != prev_label:
            flush()
            grouped = defaultdict(list)
        for x, ys in per_x.items():
            grouped[x].append(sum(ys) / len(ys))
        prev_label = label

    if grouped:
        flush()
    return series


def axis_letter(index, letter_arg):
    if letter_arg < 0:
        return ""
    offset = index if letter_arg == 0 else letter_arg - 1
    return rf"$\mathbf{{({chr(ord('a') + offset)})}}$ "


def row_major(handles, labels, ncols):
    # https://stackoverflow.com/questions/66783109
    return (sum((seq[i::ncols] for i in range(ncols)), []) for seq in (handles, labels))


def finish_axis(ax):
    ax.autoscale_view()
    ax.tick_params(axis='both', which='major', labelsize=ticksize)
    ax.tick_params(axis='both', which='minor', labelsize=ticksize)
    ymin, ymax = ax.get_ylim()
    ax.set_ylim(ymin, ymax + 0.05 * (ymax - ymin))
    ax.spines[['right', 'top']].set_visible(False)
    ax.yaxis.set_tick_params(pad=1)

# plot branches

def plot_write_amplification_bars(ax, file, filename, letter):
    groups = defaultdict(list)
    labels = []
    for line in file:
        label, data_str = parse_line(line)
        groups[label].append(float(data_str[0]))
        if label not in labels:
            labels.append(label)

    for label in labels:
        mean = np.mean(groups[label]) * 1000000 / BAR_PLOT_TOTAL_INSERTS
        ax.bar(label, mean, color=PLOT_STYLE_KWARGS[label]["color"])

    ax.set_xlabel(letter + r'growth coefficient ($r$)', fontsize=fontsize)
    ax.set_ylabel(YLABELS[filename], fontsize=fontsize, labelpad=1)
    ax.set_xticks(labels)
    ax.set_xticklabels([label.split("_", 1)[0][-1] for label in labels])
    ax.spines[['right', 'top']].set_visible(False)
    ax.yaxis.set_tick_params(pad=0)
    ax.tick_params(axis='both', which='major', labelsize=ticksize)
    ax.tick_params(axis='both', which='minor', labelsize=ticksize)


def plot_wiredtiger_size(ax, file, filename, letter):
    legend_style["handlelength"] = 1
    legend_style["fontsize"] = 12

    seen = set()
    nofilter_lines = []
    zeno_end = 0
    curve_end = 0
    for line in file:
        label, data_str = parse_line(line)
        if label == "NOFILTER":
            if len(data_str) % 2 != 0:
                continue
            vals = [float(v) for v in data_str]
            nofilter_lines.append([(vals[i] / 1000.0, vals[i + 1]) for i in range(0, len(vals), 2)])
            continue
        if label in seen or label == "IZF2":
            continue
        seen.add(label)
        data = [float(v) for v in data_str]
        x = [i * 0.001 for i in range(len(data))]
        if x:
            curve_end = max(curve_end, x[-1])
            if label in ("IZF", "VZF"):
                zeno_end = max(zeno_end, x[-1])
        ax.plot(x, data, **PLOT_STYLE_KWARGS[label],
                **{**LINES_STYLE, "linewidth": 0.8, "markevery": -1})

    # Cut the plot where the Zeno filters finish so slower baselines do not stretch the
    # x axis with a lone tail.
    right_lim = zeno_end if zeno_end > 0 else curve_end

    # Merge the baseline lines into a single step curve, ignoring truncated lines.
    max_buf_size = 0
    if nofilter_lines:
        common_len, _ = Counter(len(pairs) for pairs in nofilter_lines).most_common(1)[0]
        valid_lines = [pairs for pairs in nofilter_lines if len(pairs) == common_len]
        x = [min(pairs[i][0] for pairs in valid_lines) for i in range(common_len)]
        y = [max(pairs[i][1] for pairs in valid_lines) for i in range(common_len)]
        max_buf_size = y[-1]

        # No label: the capacity line is annotated with text, and per-segment labels would
        # otherwise add one legend entry per segment.
        style = {**PLOT_STYLE_KWARGS["NOFILTER"], "marker": None, "linewidth": 0.8,
                 "color": '#121212', "linestyle": '--', "label": None}
        for i in range(len(x) - 1):
            ax.plot([x[i], x[i + 1]], [y[i], y[i]], **style)
        # The last capacity sample is taken at the final expansion; extend the line so
        # it spans the visible range.
        if right_lim > x[-1]:
            ax.plot([x[-1], right_lim], [y[-1], y[-1]], **style)

    ax.text(right_lim * 0.3, max_buf_size + 0.3, "memory capacity",
            ha='left', va='bottom', fontsize=10)
    ax.set_xlabel(letter + XLABELS[filename], fontsize=fontsize)
    ax.set_ylabel(YLABELS[filename], fontsize=fontsize, labelpad=1)
    ax.set_yscale('linear')
    ax.set_xlim(left=0, right=right_lim if right_lim > 0 else None)
    ax.set_ylim(bottom=0)
    ax.spines[['right', 'top']].set_visible(False)
    ax.yaxis.set_tick_params(pad=0)


def plot_size_over_time(ax, file, filename, letter):
    marker_stride = MARKER_STRIDE_OVERALL if filename == "exp_overall_size" else MARKER_STRIDE
    seen = set()
    target_len = 0
    for line in file:
        label, data_str = parse_line(line)
        if label in seen:
            continue
        seen.add(label)
        data = [max(0.1, float(v)) for v in data_str]
        data = [v - data[0] for v in data]
        x = [i * 0.01 for i in range(len(data))]
        if "VZF" in label:
            target_len = len(data)
        # Extend prematurely ended lines with their last value.
        if target_len > len(data) and filename == "ie_size":
            last_val = data[-1] if data else 0
            data.extend([last_val] * (target_len - len(data)))
            x.extend([i * 0.1 for i in range(len(x), target_len)])
        ax.plot(x, data, **PLOT_STYLE_KWARGS[label],
                **{**LINES_STYLE, "linewidth": 1, "markevery": marker_stride})

    ax.set_xlabel(letter + XLABELS[filename], fontsize=fontsize)
    ax.set_ylabel(YLABELS[filename], fontsize=fontsize, labelpad=1)
    ax.set_yscale('linear')
    ax.set_ylim(bottom=0)
    if filename == "exp_overall_size":
        ax.set_ylim(bottom=0, top=1400)
        ax.set_xlim(left=0)
        legend_style["handlelength"] = 2
        legend_style["fontsize"] = choose(singlecolumn, *SMALL_LEGEND_FONT_SIZE)
    ax.spines[['right', 'top']].set_visible(False)
    ax.yaxis.set_tick_params(pad=0)
    ax.tick_params(axis='both', which='major', labelsize=ticksize)
    ax.tick_params(axis='both', which='minor', labelsize=ticksize)


def plot_size_ratio(ax, file, filename, letter):
    series = read_averaged_series(file)
    (_, x1, y1), (_, _, y2) = series[0], series[1]
    ratios = [a / b if a != 0 else float('nan') for a, b in zip(y1, y2)]
    ax.plot(x1, ratios, marker='o', linestyle='-', color='#404040', **LINES_STYLE)

    ax.set_xlabel(letter + XLABELS.get(filename, "# entries"), fontsize=fontsize)
    if filename == "exp_expand_ratio":
        ax.set_ylabel(YLABELS[filename], fontsize=fontsize, labelpad=1)
    ax.set_xscale('linear')
    ax.set_ylim(top=1.0)
    finish_axis(ax)
    ax.yaxis.set_major_formatter(FormatStrFormatter('%.2f'))


def plot_series(ax, file, filename, letter):
    skip_labels = ("IZF2",) if filename == "exp_wiredtiger_insert" else ()
    series = read_averaged_series(file, skip_labels)

    axins = None
    if filename == "fe_size":
        axins = inset_axes(ax, width="50%", height="50%", loc="upper left")
        axins.set_xscale('log')
        axins.tick_params(axis='x', which='both', bottom=False, labelbottom=False)
        axins.tick_params(axis='y', which='both', left=False, labelleft=False)
        axins.set_xlim(*FE_SIZE_INSET_XLIM)
        axins.set_ylim(*FE_SIZE_INSET_YLIM)
        smallbox, con1, con2 = mark_inset(ax, axins, loc1=1, loc2=3)
        smallbox.set_linewidth(0.8)
        smallbox.set_zorder(15)
        for con in (con1, con2):
            con.set_linestyle("--")
            con.set_linewidth(0.8)

    for label, x_values, y_values in series:
        style = PLOT_STYLE_KWARGS[label]
        if args.running_average != 0 and len(x_values) >= args.running_average:
            smoothed_y = running_average(y_values, args.running_average)
            ax.plot(x_values[:len(smoothed_y)], smoothed_y, **style, **{**LINES_STYLE,
                    "linewidth": 0.8 if "concurrent" in filename else LINES_STYLE["linewidth"]})
        elif filename == "fe_size":
            ax.step(x_values, y_values, **style, **LINES_STYLE, where='post')
            axins.step(x_values, y_values, **style, **LINES_STYLE, where='post')
        else:
            ax.plot(x_values, y_values, **style, **{**LINES_STYLE,
                    "markersize": 0 if filename in NO_MARKER_FILES else LINES_STYLE["markersize"]})

    ax.set_xlabel(letter + XLABELS.get(filename, "# entries"), fontsize=fontsize)
    ax.set_ylabel(YLABELS[filename], fontsize=fontsize, labelpad=1)

    if any(sub in filename.lower() for sub in LINEAR_X_SUBSTRINGS):
        ax.set_xscale('linear')
    else:
        ax.set_xscale('log')

    if any(sub in filename.lower() for sub in LOG_Y_SUBSTRINGS):
        ax.set_yscale('log')
    else:
        ax.set_yscale('linear')
        ax.set_ylim(bottom=0)

    if filename == "fe_insert":
        ax.set_ylim(bottom=0)

    if filename in ["exp_concurrency_one", "exp_concurrency"]:
        ax.set_xscale('log')
        ax.set_xticks(THREAD_COUNT_XTICKS)
        ax.set_xticklabels([str(t) for t in THREAD_COUNT_XTICKS])
        ax.xaxis.set_minor_locator(NullLocator())

    if filename == "exp_directory_size":
        ax.axhline(y=1, color='#1f77b4', linestyle='--', linewidth=1)
        ax.text(2**9, 0.9, "L2 cache size", ha='left', va='bottom',
                fontsize=choose(singlecolumn, *SMALL_LEGEND_FONT_SIZE))
        legend_style["fontsize"] = choose(singlecolumn, *SMALL_LEGEND_FONT_SIZE)
        legend_style["handlelength"] = 1

    if filename in ["exp_wiredtiger_insert", "exp_wiredtiger_nquery", "exp_wiredtiger_pquery"]:
        def frac_formatter(x, pos):
            return "0" if x == 0 else str(Fraction(x).limit_denominator())
        ax.xaxis.set_major_formatter(FuncFormatter(frac_formatter))
        ax.set_xticks(FRACTION_XTICKS)
        ax.xaxis.set_minor_locator(NullLocator())

    if individual_legend:
        legend_style["fontsize"] = 12
        legend_style["handlelength"] = 1
        if filename == "exp_directory_query":
            loc, bbox, markerfirst = 'upper left', (-0.05, 1.0), True
        else:
            loc, bbox, markerfirst = 'lower right', (1.05, -0.05), False
        ax.legend(loc=loc, ncol=1, bbox_to_anchor=bbox, frameon=False,
                  fontsize=legend_style["fontsize"] - 1, columnspacing=0.1,
                  handlelength=legend_style["handlelength"], markerfirst=markerfirst,
                  handletextpad=0.3, labelspacing=0.1)

    finish_axis(ax)

# entry point

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
parser.add_argument('--narrowplot', action='store_true')
parser.add_argument('--individual_legend', action='store_true')
parser.add_argument('--running_average', type=int, default=0,
                    help="Window size for running average (0 disables it)")
parser.add_argument('--max_columns', type=int, default=4,
                    help="Maximum number of columns")
parser.add_argument('--legend_column', type=int, default=0,
                    help="Column of the axis for the legend")
parser.add_argument('--legend_ncols', type=int, default=0,
                    help="Number of columns in the legend")
parser.add_argument('--letter', type=int, default=0, help="Embed alphabet letters in x axis")
parser.add_argument('--singlecolumn', action='store_true')
args = parser.parse_args()

filenames = args.filenames
wideplot = args.wideplot
narrowplot = args.narrowplot
singlecolumn = args.singlecolumn
individual_legend = args.individual_legend
no_legend = args.no_legend or individual_legend

LINES_STYLE = choose(singlecolumn, SINGLE_LINES_STYLE, DOUBLE_LINES_STYLE)

# Figure geometry.
n_files = len(filenames)
n_rows = math.ceil(n_files / args.max_columns)
num_cols = min(n_files, args.max_columns)

width_px = COLUMN_WIDTH * num_cols
height_px = choose(singlecolumn, *ROW_HEIGHT) * n_rows
if wideplot:
    width_px = 2 * width_px
    height_px = WIDE_PLOT_HEIGHT
if narrowplot:
    width_px, height_px = NARROW_PLOT_SIZE
if num_cols == 4:
    height_px = choose(singlecolumn, height_px, FOUR_COLUMN_HEIGHT)
    width_px = width_px * WIDTH_STRETCH
elif num_cols == 2:
    width_px = width_px * WIDTH_STRETCH
if no_legend:
    height_px = choose(singlecolumn, *NO_LEGEND_HEIGHT)

# Font sizes.
fontsize = choose(singlecolumn, *FONT_SIZE)
ticksize = choose(singlecolumn, *TICK_SIZE)
legend_style = {"fontsize": choose(singlecolumn, *LEGEND_FONT_SIZE), "handlelength": 2}
if num_cols <= 2 and wideplot:
    legend_style["fontsize"] = WIDE_LEGEND_FONT_SIZE
if narrowplot:
    fontsize = NARROW_FONT_SIZE
    ticksize = NARROW_TICK_SIZE

fig, axes = plt.subplots(n_rows, num_cols, figsize=(width_px / DPI, height_px / DPI), dpi=DPI,
                         constrained_layout=True)
if n_files == 1:
    axes = [axes]
if num_cols != 1:
    axes = axes.flatten()

for idx, (ax, filename) in enumerate(zip(axes, filenames)):
    letter = axis_letter(idx, args.letter)
    for spine in ax.spines.values():
        spine.set_zorder(20)

    with open(RESULTS_DIR + filename + ".csv", 'r') as file:
        if filename in BAR_PLOT_FILES:
            plot_write_amplification_bars(ax, file, filename, letter)
        elif filename == "exp_wiredtiger_size":
            plot_wiredtiger_size(ax, file, filename, letter)
        elif filename in SIZE_OVER_TIME_FILES:
            plot_size_over_time(ax, file, filename, letter)
        elif filename in RATIO_FILES:
            plot_size_ratio(ax, file, filename, letter)
        else:
            plot_series(ax, file, filename, letter)

# Shared legend above the figure.
offset_px = LEGEND_OFFSET_PX
if wideplot or args.legend_ncols != 0:
    offset_px = LEGEND_OFFSET_WIDE_PX

handles, labels = axes[args.legend_column].get_legend_handles_labels()

# Number of legend columns: an explicit --legend_ncols wins; otherwise fit all entries on
# a single row (up to MAX_LEGEND_NCOLS per row) so the legend never wraps into the axes.
legend_ncols = args.legend_ncols if args.legend_ncols else max(1, min(len(labels), MAX_LEGEND_NCOLS))
if wideplot:
    legend_ncols = MAX_LEGEND_NCOLS

if len(labels) > legend_ncols:
    # The legend wraps onto a second row; reorder the entries to be row major.
    offset_px = LEGEND_OFFSET_WRAPPED_PX
    if not no_legend:
        offset_norm = offset_px / height_px
        fig.legend(*row_major(handles, labels, legend_ncols), loc='upper center',
                   bbox_to_anchor=(0.5, 1 + offset_norm), ncol=legend_ncols, frameon=False,
                   fontsize=legend_style["fontsize"], columnspacing=0.8,
                   handlelength=legend_style["handlelength"])
elif not no_legend:
    offset_norm = offset_px / height_px
    fig.legend(handles, labels, loc='upper center',
               bbox_to_anchor=(0.5, 1 + offset_norm), ncol=legend_ncols, frameon=False,
               fontsize=legend_style["fontsize"], columnspacing=1,
               handlelength=legend_style["handlelength"])

if n_rows > 1:
    plt.subplots_adjust(hspace=HSPACE)

extension = '.png' if args.png else '.pdf'
plt.savefig(RESULTS_DIR + args.savefilename + extension, bbox_inches='tight', pad_inches=0)
