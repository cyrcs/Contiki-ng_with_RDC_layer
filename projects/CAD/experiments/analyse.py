import numpy as np
from scipy import stats
import pandas as pd
import matplotlib.pyplot as plt
# read from file 'results.txt'
with open("test_20m.txt", "r") as file:
    arr = file.readlines()

results = []
arr_actual_cads = [
    52, 46, 37, 26, 17, # SF7 BW1600
    169, 119, 74, 42, 23, # SF7 BW200
    98, 75, 51, 30, 17, # SF9 BW1600
    168, 110, 65, 36, 19, # SF9 BW200
    138, 93, 56, 31, 17,# SF12 BW1600
    150, 98, 58, 32,18 # SF12 BW200
]    
amount_of_packets = 10
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

print(df)

# Grouping every 5 rows by the "CAD" column
grouped_df = df.groupby(df.index // 5)
print(grouped_df)

# Calculating the width of each bar group
bar_width = 0.15

# Creating a figure and axis object
fig, ax = plt.subplots()

# Iterating over groups
for i, (name, group) in enumerate(grouped_df):
    # Calculate x positions for bars

    x_positions = np.arange(len(group['CAD'])) + i * (bar_width)

    # Plotting bars for each group
    ax.bar(x_positions, group['Sample Mean'], bar_width, label=f' {group["SF"].iloc[0][:-1]}{group["BW"].iloc[0][:-1]}')

    for i in range(len(x_positions)):
        ax.text(x_positions[i], group['Sample Mean'].iloc[i], f'{group["Sample Mean"].iloc[i]:.2f}', ha='center', va='bottom')


    plt.xticks(x_positions - 5 * 0.15, group['CAD'])
# Setting labels and title
plt.ylabel('Sample Mean')
plt.title('Sample Mean for every mode(SF and BW) for every amount of CAD symbols')
plt.xlim(-0.2, 5.44)

# Adding legend
ax.legend()

plt.show()