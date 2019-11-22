#!/usr/bin/python

import sys
import getopt
import os
import json

PLAFORM_SH_MEMBERS = "Jianxin Pan, Tao Zeng, Hanjie Lin, Yan Wang, Jiamin Ma, Jianxiong Pan, \
		   Qi Duan, Zhuo Wang, Yue Wang, He He, Yu Tu, \
		   Qiufang Dai, Hong Guo, Zhiqiang Liang, Pan Yang, Shunzhou Jiang, Huan Biao"

def usage():
	print("\nweekly_change.py is a tool assisting at generating weekly change report")
	print("usage:")
	print("-s xxxx-xx-xx")
	print("    specify the the starting date")
	print("-e xxxx-xx-xx")
	print("    specify the the ending date")
	print("-p")
	print("    highlight commit from SH PLATFORM MEMBER(kernel)")
	print("-h")
	print("    show this help info")

def exec_cmd_rdata(cmd):
    result = os.popen(cmd)
    res = result.read()
    res=res.strip('\n')

    return res

def is_sh_platform_commit(auther):
	if auther in PLAFORM_SH_MEMBERS:
		return True
	return False

def main():
	start_date = ""
	end_date = ""
	git_commit_cmd = "git log --pretty=\"%h - %s\" --since="
	git_commit_info = ""
	html_out = "<div>"
	gerrit_cmd_1 = "ssh -p 29418 scgit.amlogic.com gerrit query --format=JSON commit:"
	highlight_shp = False
	opts, args = getopt.getopt(sys.argv[1:], "hps:e:")
	for op, value in opts:
		if op == "-s":
			start_date = value
		if op == "-e":
			end_date = value
		if op == "-p":
			highlight_shp = True
		if op == "-h":
			usage()
			sys.exit()

	#print(start_date)
	git_commit_cmd = git_commit_cmd+start_date
	if end_date == "":
		git_commit_cmd = git_commit_cmd + " --no-merges"
	else:
		git_commit_cmd = git_commit_cmd + " --until=" + end_date + " --no-merges"
	#print(git_commit_cmd)
	git_commit_info = exec_cmd_rdata(git_commit_cmd)
	#print(git_commit_info)

	# obtain gerrit url via gerrit query
	for line in git_commit_info.splitlines():
		#print(line)
		commit = line.split(" - ")[0]
		message = line.split(" - ")[1]
		#print(commit+"-"+message)
		gerrit_cmd = gerrit_cmd_1 + commit
		#print(gerrit_cmd)
		gerrit_info = exec_cmd_rdata(gerrit_cmd)
		gerrit_info = gerrit_info.split("\n")[0]
		#print(gerrit_info)
		data = json.loads(gerrit_info)
		#print(data['owner']["name"])
		#print(data['url'])
		owner_name = data['owner']["name"]
		gerrit_url = data['url']

		#
		# we do not use any html library, which is too heavy
		#
		html_out = html_out + '<a title=\"' + gerrit_url + \
			   '\"ass=\" external\" rel=\"external nofollow\" href=\"' + gerrit_url + '\" target=\"_blank\">' + \
			   commit + '</a> - ' + message
		if highlight_shp == True:
			if is_sh_platform_commit(owner_name) == True:
				html_out += ' (SH_PLATFORM_KERNEL)<br>'
			else:
				html_out += '<br>'
		else:
			html_out += '<br>'
		#print(html_out)
	html_out = html_out + '</div>'
	#print(html_out)

	html = open("weekly_update.html", "w+")
	html.write(html_out);
	html.close

if __name__ == "__main__":
        main()

