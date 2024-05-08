import numpy as np
from scipy import stats
import pandas as pd
import matplotlib.pyplot as plt

# list_of_files = ["test_1m.txt", "test_5m.txt", "test_10m.txt", "test_15m.txt", "test_18m.txt", "test_20m.txt", "test_30m.txt"]
# list_of_files = ["test_10m_indoor_ground.txt", "test_20m_indoor_ground.txt",
# "test_30m_indoor_ground_door_closed.txt","test_30m_indoor_chair.txt", "test_30m_indoor_chair_door_closed.txt"]
list_of_files = ["test_10m_indoor.txt", "test_27m_indoor.txt", "test_45m_indoor.txt", "test_45m_indoor_2.txt","test_52m_indoor.txt", "test_58m_indoor.txt"]
folder = "indoor D3/"
arr_actual_cads = [
    52, 46, 37, 27, 17, # SF7 BW1600
    169, 119, 74, 42, 23, # SF7 BW200
    98, 75, 51, 31, 17, # SF9 BW1600
    168, 110, 65, 36, 19, # SF9 BW200
    138, 93, 56, 32, 17,# SF12 BW1600
    150, 98, 58, 32,18 # SF12 BW200
]   
amount_of_packets = 10

def read_file(file_name, folder):
    print(file_name)
    with open(file_name, "r") as file:
        arr = file.readlines()
    
    results = []
    i = 0
    SF = ""
    BW = ""
    CAD = ""
    jumps = 0
    while i < len(arr):
    
        if arr[i].startswith("SF"):
            SF = arr[i]
            BW = arr[i + 1]
            CAD = arr[i + 2]
            i += 3
            jumps += 3
        elif arr[i].startswith("CAD"):
            CAD = arr[i]
            i += 1
            jumps += 1

        arr_avg = []
        while i < len(arr) and not arr[i].startswith("SF") and not arr[i].startswith("CAD"):
            arr_avg.append(arr[i].count("1") / arr_actual_cads[(i - jumps) //amount_of_packets])
            i += 1
    
        averages = arr_avg
        # Calculate the sample mean and standard error of the mean (SEM)
        sample_mean = np.mean(averages)
        sem = stats.sem(averages)

        # Calculate the degrees of freedom (n-1 for a sample)
        df = len(averages) - 1

        # Calculate the critical value for a 95% confidence interval
        critical_value = stats.t.ppf(0.975, df)  # For a two-tailed test

        # Calculate the margin of error
        margin_of_error = critical_value * sem

        # Calculate the confidence interval
        lower_bound = max(0, sample_mean - margin_of_error)
        upper_bound = min(1,sample_mean + margin_of_error)

        results.append([SF,
                BW,
                CAD,    
                sample_mean, 
                sem, 
                df, 
                critical_value, 
                margin_of_error, 
                lower_bound, 
                upper_bound])

    df = pd.DataFrame(results, columns=["SF", "BW", "CAD", "Sample Mean", "SEM", "df", "Critical Value", "Margin of Error", "Lower Bound", "Upper Bound"])
    return df

x_names = []
bar_width = 0.15

dfs = []
for file in list_of_files:
    file_name = folder + file
    dfs.append(read_file(file_name, folder))
    x_names.append(file[5:-4])    


for i in range(30):
    step = (i % 5)
    x = np.arange(bar_width * step, len(dfs) + bar_width * step, 1)

    # Creating a figure and axis object
    if i % 5 == 0:
        fig, ax = plt.subplots()
    
    if i % 5 % 2 == 0 and not i % 5 % 4 == 0:
        print(i)
        plt.xticks(x, x_names)

    means = []
    means_label = []
    lower = []
    upper = []
    for df in dfs:
        means.append(df["Sample Mean"][i])
        if df["Sample Mean"][i] == 0:
            means_label.append(0)
        else:
            means_label.append(df["Sample Mean"][i] * 0.5)
        lower.append(abs(df["Sample Mean"][i] - df["Lower Bound"][i]))
        upper.append(abs(df["Upper Bound"][i] - df["Sample Mean"][i]))

    ax.bar(x, means, bar_width, label="CAD symbols: " + dfs[0]["CAD"][i][3:-1])

    # Add error bars
    ax.errorbar(x, means, yerr=[lower, upper], fmt='none', capsize=5, color='black')
    
    for j in range(len(x)):
        ax.text(x[j], 0, f'{means[j]:.2f}', ha='center', va='bottom', fontsize="small")

    plt.ylabel("Sample Mean")
    plt.xlim(-0.2, len(dfs)*1.15)
    plt.title(dfs[0]["SF"][i][:-1] + " " + dfs[0]["BW"][i][:-1])
    ax.axhline(y=1, color='r', linestyle='--')

    # resize window and tighten layout
    plt.get_current_fig_manager().full_screen_toggle()
    plt.tight_layout()

    # show plot if it is the fifth plot
    if i % 5 == 4:
        ax.legend()
        plt.show()
