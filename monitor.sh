#!/bin/bash

declare -i online
declare -i nowtime
declare -i filetime
declare -i difftime

for ((1;1;1))
do
	clear
	##############################1、获取cpu和内存的使用量####################################################
	#CPU Time
	declare -i user_time1=`cat /proc/stat|head -n 1|awk '{print $2}'`
	declare -i nice_time1=`cat /proc/stat|head -n 1|awk '{print $3}'`
	declare -i system_time1=`cat /proc/stat|head -n 1|awk '{print $4}'`
	declare -i idle_time1=`cat /proc/stat|head -n 1|awk '{print $5}'`
	declare -i iowait_time1=`cat /proc/stat|head -n 1|awk '{print $6}'`
	declare -i irq_time1=`cat /proc/stat|head -n 1|awk '{print $7}'`
	declare -i softirq_time1=`cat /proc/stat|head -n 1|awk '{print $8}'`
	declare -i stealstolen_time1=`cat /proc/stat|head -n 1|awk '{print $9}'`
	declare -i guest_time1=`cat /proc/stat|head -n 1|awk '{print $10}'`
	sleep 1
	declare -i user_time2=`cat /proc/stat|head -n 1|awk '{print $2}'`
	declare -i nice_time2=`cat /proc/stat|head -n 1|awk '{print $3}'`
	declare -i system_time2=`cat /proc/stat|head -n 1|awk '{print $4}'`
	declare -i idle_time2=`cat /proc/stat|head -n 1|awk '{print $5}'`
	declare -i iowait_time2=`cat /proc/stat|head -n 1|awk '{print $6}'`
	declare -i irq_time2=`cat /proc/stat|head -n 1|awk '{print $7}'`
	declare -i softirq_time2=`cat /proc/stat|head -n 1|awk '{print $8}'`
	declare -i stealstolen_time2=`cat /proc/stat|head -n 1|awk '{print $9}'`
	declare -i guest_time2=`cat /proc/stat|head -n 1|awk '{print $10}'`
	
	declare -i cpu_idle=idle_time2-idle_time1
	declare -i cpu_total=user_time2-user_time1+nice_time2-nice_time1+system_time2-system_time1+idle_time2-idle_time1+iowait_time2-iowait_time1+irq_time2-irq_time1+softirq_time2-softirq_time1+stealstolen_time2-stealstolen_time1+guest_time2-guest_time1
	cpu_use=$(echo "scale=3;100*($cpu_total-$cpu_idle+0.0)/$cpu_total"|bc -l)
	
	#memory
	declare -i total_mem=`cat /proc/meminfo|grep -w MemTotal|awk '{print $2}'`
	declare -i free_mem=`cat /proc/meminfo|grep -w MemFree|awk '{print $2}'`
	declare -i buffer_mem=`cat /proc/meminfo|grep -w Buffers|awk '{print $2}'`
	declare -i cache_mem=`cat /proc/meminfo|grep -w Cached|awk '{print $2}'`
	mem_use=$(echo "scale=3;100.0-100*($free_mem+$buffer_mem+$cache_mem+0.0)/$total_mem"|bc -l)
	
	#echo -e $user_time1 $nice_time1 $system_time1 $idle_time1 $iowait_time1 $irq_time1 $softirq_time1 $stealstolen_time1 $guest_time1
	#echo -e $user_time2 $nice_time2 $system_time2 $idle_time2 $iowait_time2 $irq_time2 $softirq_time2 $stealstolen_time2 $guest_time2
	echo -e "-----------------------------------------------------------------"
	echo -e cpu_use=$cpu_use%
	echo -e mem_use=$mem_use%
	
	#GPU
	declare -i gpu_i=0
	declare -i gpu_num=0
	
	#测试是否存在nvidia-smi命令
	declare -i nvidia_exist=`which nvidia-smi|wc -l`
	if [ $nvidia_exist -ge 1 ]
	then
		gpu_num=`nvidia-smi -L|grep GPU|wc -l`
	else
		gpu_num=0
	fi
	
	if [ $gpu_num -ge 1 ]
	then
		for((gpu_i=0;gpu_i<$gpu_num;gpu_i++))
		do
			gpu_util=`nvidia-smi -i $gpu_i --query|grep Gpu|grep %|tr -d ' '|awk -F ':|%' '{print $2}'`
			declare -i gpu_mem_total=`nvidia-smi -i $gpu_i --query|grep Total|head -n 1|tr -d ' '|awk -F ':|M' '{print $2}'`
			declare -i gpu_mem_used=`nvidia-smi -i $gpu_i --query|grep Used|head -n 1|tr -d ' '|awk -F ':|M' '{print $2}'`
			gpu_mem_use=$(echo "scale=3;100.0*($gpu_mem_used)/$gpu_mem_total"|bc -l)
			
			echo -e gpu$gpu_i gpu_util=$gpu_util%  gpu_mem=$gpu_mem_use%
		done
	fi

	#GPU end
	echo -e "-----------------------------------------------------------------"
	##############################2、获取每一路编码的状态 ####################################################
	PIDLIST=`ls -1 /tmp|grep "avencoder."|awk  -F '.' '{print $2}'|sort -n`
	online=0
	
	echo -e "index \t destaddr \t\t realbitrate(bps) \t realfps"
	for p in $PIDLIST
	do
		nowtime=`date '+%F-%T' | awk -F- '{print $1$2$3$4}' | awk -F: '{print $1$2$3}'`
		filetime=`stat /tmp/avencoder.$p | grep -i Modify | awk -F. '{print $1}' | awk '{print $2$3}'| awk -F- '{print $1$2$3}' | awk -F: '{print $1$2$3}'`
		difftime=nowtime-filetime

		if [ $difftime -ge 5 ]
		then
			sudo rm -rf "/tmp/avencoder.$p"
		else
			index=`cat /tmp/avencoder.$p |grep index |awk -F '=' '{print $2}'|tr -d '\n'` 
			destaddr=`cat /tmp/avencoder.$p |grep destaddr |awk -F '=' '{print $2}'|sed -n 1p |tr -d '\n'` 
			realbitrate=`cat /tmp/avencoder.$p |grep realbitrate |awk -F '=' '{print $2}'|sed -n 1p |tr -d '\n'` 
			realfps=`cat /tmp/avencoder.$p |grep realfps |awk -F '=' '{print $2}'|tr -d '\n'` 
			
			echo -e "$index \t $destaddr \t $realbitrate \t\t $realfps"
			online=$online+1;
		fi
	done
	echo -e "-----------------------------------------------------------------"
	echo -e "online:$online"
	sleep 1
done