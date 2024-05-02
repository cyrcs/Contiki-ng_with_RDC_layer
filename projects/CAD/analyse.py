import numpy as np
from scipy import stats
import pandas as pd
# read from file 'results.txt'
with open("results_2.txt", "r") as file:
    arr = file.readlines()

results = []
arr_actual_cads = [150, 98, 58, 32,18,138,93, 56,31,17,165,118,74,42,23,51,45,37,27,17]
amount_of_packets = 10
arr_test = []
i = 0
while i < len(arr):
    arr_test.append(arr[i][:-1])
    print(arr_test[i // (amount_of_packets + 1)])

    i += 1

    arr_avg = []
    while i < len(arr) and not arr[i].startswith("Testing"):
        arr_avg.append(arr[i].count("1") / arr_actual_cads[i //(amount_of_packets + 1)])
        i += 1
    # print(f"The mean is: {np.mean(arr_avg):.2f} and the std is: {np.std(arr_avg):.2f}")

    # Define the sample data (10 averages)
    # averages = np.array([10.2, 11.5, 9.8, 12.3, 10.9, 11.1, 10.5, 11.8, 10.6, 11.2])
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

    results.append([arr_test[i // (amount_of_packets + 1) - 1], 
            sample_mean, 
            sem, 
            df, 
            critical_value, 
            margin_of_error, 
            lower_bound, 
            upper_bound])


    
print(pd.DataFrame(results, columns=["Test", "Sample Mean", "SEM", "df", "Critical Value", "Margin of Error", "Lower Bound", "Upper Bound"]))