from scipy import stats
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import os

actual_cads = {
    "SF7 BW1600": {
        "CAD1": 51,
        "CAD2": 45,
        "CAD4": 37,
        "CAD8": 26,
        "CAD16": 17
    },
    "SF7 BW200": {
        "CAD1": 169,
        "CAD2": 119,
        "CAD4": 74,
        "CAD8": 42,
        "CAD16": 23
    },
    "SF9 BW1600": {
        "CAD1": 98,
        "CAD2": 75,
        "CAD4": 51,
        "CAD8": 31,
        "CAD16": 17
    },
    "SF9 BW200": {
        "CAD1": 168,
        "CAD2": 110,
        "CAD4": 65,
        "CAD8": 36,
        "CAD16": 19
    },
    "SF12 BW1600": {
        "CAD1": 138,
        "CAD2": 93,
        "CAD4": 57,
        "CAD8": 32,
        "CAD16": 17
    },
    "SF12 BW200": {
        "CAD1": 150,
        "CAD2": 98,
        "CAD4": 58,
        "CAD8": 32,
        "CAD16": 18
    }
}

folder = "outdoor_watertoren_down/"
list_of_files = sorted(os.listdir(folder))

if __name__ == "__main__":
# if __name__ == True:
    results = []
    for file in list_of_files:
        print(file)
        with open(folder + file, "r") as f:
            arr = f.readlines()

        SF = ""
        BW = ""
        CAD = ""
        correct_packets = ""
        # d = file[8:-5]
        d = file[5:-4]

        i = 0
        while i < len(arr):
            # if line specifies the SF => always followed by BW and CAD
            if arr[i].startswith("SF"):
                SF = arr[i][:-1]
                i += 1
                continue
            elif arr[i].startswith("BW"):
                BW = arr[i][:-1]
                i += 1
                continue
            elif arr[i].startswith("CAD"):
                CAD = arr[i][:-1]
                i += 1
                continue
            elif arr[i].startswith("Correct packet received:"):
                correct_packets = f"{arr[i].count('1')}/10" 

            elif not arr[i].startswith("0") and not arr[i].startswith("1"):
                i += 1
                print("skipping line: not 0 or 1: ", arr[i - 1])
                continue

            # if line starts with 0 or 1 => valid line => process it
            avgs = []
            while i < len(arr) and (arr[i].startswith("0") or arr[i].startswith("1")):
                avgs.append(arr[i].count("1") / actual_cads[SF + " " + BW][CAD])
                i += 1
            
            # process averages
            sample_mean = np.mean(avgs)
            sem = stats.sem(avgs)
            df = len(avgs) - 1
            critical_value = stats.t.ppf(0.975, df)
            margin_of_error = critical_value * sem
            lower_bound = max(0, sample_mean - margin_of_error)
            upper_bound = min(1, sample_mean + margin_of_error)

            # append results
            results.append([d, SF, BW, CAD, correct_packets, sample_mean, sem, df, critical_value, margin_of_error, lower_bound, upper_bound])


    # create dataframe
    df = pd.DataFrame(results, columns=["distance", "SF", "BW", "CAD", "correct_packets", "sample_mean", "sem", "df", "critical_value", "margin_of_error", "lower_bound", "upper_bound"])

    print(df)

    # plot data for each combo of SF and BW
    for sf in df["SF"].unique():
        for bw in df["BW"].unique():
            data = df[(df["SF"] == sf) & (df["BW"] == bw)]
            print(data)

            cads_to_plot = len(data["CAD"].unique())
            distances_to_plot = len(data["distance"].unique())
            bar_width = 0.8 / cads_to_plot

            # Create x axis
            x = np.arange(bar_width, distances_to_plot,1)
            print(x)
            fig, ax = plt.subplots() 
            for i, cad in enumerate(data["CAD"].unique()):
                print(i)
                print(cad)
                mean = (data[data["CAD"] == cad]["sample_mean"]).to_numpy()
                lower_bound = abs(mean - (data[data["CAD"] == cad]["lower_bound"]).to_numpy())
                upper_bound = abs((data[data["CAD"] == cad]["upper_bound"]).to_numpy() - mean)

                print("mean: ")
                print(mean)
                print(x)
                print(x + i * bar_width)

                # plot bars
                ax.bar(x + i * bar_width, mean, bar_width, label=cad)
                # plot error bars
                ax.errorbar(x + i * bar_width, mean,  yerr=[lower_bound, upper_bound], fmt='none', capsize=5, color="black")
                # plot label for mean
                for j in range(len(x)):
                    ax.text(x[j] + i * bar_width, 0, f'{mean[j]:.2f}', ha='center', va='bottom', fontsize="small")
            
            # horizontal line at y=1
            ax.axhline(y=1, color='r', linestyle='--')

            # set limits
            plt.xlim(-0.2, distances_to_plot)

            # set labels and title
            plt.title(sf + " " + bw)
            plt.ylabel("Sample Mean")
            plt.xlabel("Distance")
            plt.legend()

            # set ticks
            ax.set_xticks(x + (cads_to_plot // 2) * (bar_width / 2) + bar_width)
            # ax.set_xticklabels(data["distance"].unique() + "m")
            ax.set_xticklabels(data["distance"].unique())

            # resize window and tighten layout
            plt.get_current_fig_manager().full_screen_toggle()
            plt.tight_layout()

            plt.show()







            

