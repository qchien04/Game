import numpy as np
import scipy.stats as st

data = [11, 12, 12.5, 13, 13, 15.5, 16, 17, 22, 23, 25, 26, 
        27, 28, 28, 29, 30, 32, 33, 33, 34, 34]

a = st.norm.interval(alpha=0.95, loc=np.mean(data), scale=st.sem(data))
print("Trung bình:", mean)
print("Khoảng tin cậy 95%: (%.2f , %.2f)" % (ci_lower, ci_upper))
