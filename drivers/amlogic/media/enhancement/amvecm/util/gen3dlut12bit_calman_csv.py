# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
import sys
import csv
import struct

with open(sys.argv[1])as f:
    counter=0
    f_csv = csv.reader(f)
    calman_ver = f_csv.next()
    print(calman_ver[0])
    lut_date = f_csv.next()
    print(lut_date[0])
    lut_size = f_csv.next()
    lut_bitdepth = f_csv.next()
    lut_order = f_csv.next()
    print(lut_size[0])
    print(lut_bitdepth[0])
    print(lut_order[0])
    with open('3dlut.bin', 'ab')as fb:
        for row in f_csv:
            if counter==0:
                data0_f = 4095.0*float(row[3])
                data1_f = 4095.0*float(row[4])
                data2_f = 4095.0*float(row[5])
                counter=1
            elif counter==1:
                data3_f = 4095.0*float(row[3])
                data4_f = 4095.0*float(row[4])
                data5_f = 4095.0*float(row[5])
                data0 = int(data0_f)
                data1 = int(data1_f)
                data2 = int(data2_f)
                data3 = int(data3_f)
                data4 = int(data4_f)
                data5 = int(data5_f)
                #print("%d, %d, %d" %(data0, data1, data2))
                #print("%d, %d, %d" %(data3, data4, data5))
                combine0 = data0 | (data1 << 12)
                combine1 = data2 | (data3 << 12)
                combine2 = data4 | (data5 << 12)
                counter=0
                a = struct.pack('BBBBBBBBB', combine0 & 0xff, (combine0 >> 8) & 0xff, (combine0 >> 16) & 0xff, combine1 & 0xff, (combine1 >> 8) & 0xff, (combine1 >> 16) & 0xff, combine2 & 0xff, (combine2 >> 8) & 0xff, (combine2 >> 16) & 0xff)
                fb.write(a)

        if counter==1:
             data0 = int(data0_f)
             data1 = int(data1_f)
             data2 = int(data2_f)
             data3 = 4095
             data4 = 4095
             data5 = 4095
             print("%d, %d, %d" %(data0, data1, data2))
             print("%d, %d, %d" %(data3, data4, data5))
             combine0 = data0 | (data1 << 12)
             combine1 = data2 | (data3 << 12)
             combine2 = data4 | (data5 << 12)
             a = struct.pack('BBBBBBBBB', combine0 & 0xff, (combine0 >> 8) & 0xff, (combine0 >> 16) & 0xff, combine1 & 0xff, (combine1 >> 8) & 0xff, (combine1 >> 16) & 0xff, combine2 & 0xff, (combine2 >> 8) & 0xff, (combine2 >> 16) & 0xff)
             fb.write(a)

print('done')
