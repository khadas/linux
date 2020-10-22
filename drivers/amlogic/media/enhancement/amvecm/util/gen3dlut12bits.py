# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
import sys
import struct

maxval = 4095
step = (maxval+1) / 16
with open('3dlut.bin', 'ab')as fb:
        for m in range(0,17*17*17+1,2):
		d0 = m / 289
		d1 = (m - d0*289) / 17
		d2 = m - d0*289 - d1*17
		data_0 = min(maxval,d2*step) & 0xfff
		data_1 = min(maxval, d1*step) & 0xfff
		data_2 = min(maxval, d0*step) & 0xfff
                d0 = (m+1) / 289
                d1 = (m+1 - d0*289) / 17
                d2 = m+1 - d0*289 - d1*17
                data_3 = min(maxval,d2*step) & 0xfff
                data_4 = min(maxval, d1*step) & 0xfff
                data_5 = min(maxval, d0*step) & 0xfff
                #print(data_0)
		#print(data_1)
		#print(data_2)
		#print(data_3)
		#print(data_4)
		#print(data_5)
		if sys.argv[1] == 'bypass':
	                combine0 = data_0 | (data_1 << 12)
        	        combine1 = data_2 | (data_3 << 12)
			combine2 = data_4 | (data_5 << 12)
		else:
                	combine0 = int(sys.argv[1]) | (int(sys.argv[2]) << 12)
                	combine1 = int(sys.argv[3]) | (int(sys.argv[1]) << 12)
                	combine2 = int(sys.argv[2]) | (int(sys.argv[3]) << 12)
		#print(combine0)
		#print(combine1)
		#print(combine2)
		#print("---------------------------------")
                a = struct.pack('BBBBBBBBB', combine0 & 0xff, (combine0 >> 8) & 0xff, (combine0 >> 16) & 0xff, combine1 & 0xff, (combine1 >> 8) & 0xff, (combine1 >> 16) & 0xff, combine2 & 0xff, (combine2 >> 8) & 0xff, (combine2 >> 16) & 0xff)
		fb.write(a)
print('done')
