#!/bin/bash


ncores=0


get_gpubusid()
{

    local gpuits
    local busids
    local ids

    if [ -e /proc/driver/nvidia/gpus/  ];then
        
        gpuits=/proc/driver/nvidia/gpus/* 
    else
        echo Can not find nVidia drivers! You must install nVidia proprietary drivers properly..
        return -1;
    fi

    busids=$(echo $gpuits |tr ' ' '\n'|sed -n 's#.*:\([0-9]*\):.*#\1#p')
    

    for i in $busids;do
        ids=$ids echo "ibase=16; $i" |bc
    done

#    echo $ids
    return $ids

}



config_X(){

    local busids
    local preffix
    local id
    local file

    preffix='/etc/X11/xorg.conf.'

    busids=$(get_gpubusid)

#    echo BUSIDS: $busids
#    echo ----

    local i
    i=0
    for id in $busids;do

#        echo $id

        file=$preffix$i
#        echo file:: $file
        if [ ! -e $file ] ;then
#            echo file:$file  is not exists
            sudo nvidia-xconfig --busid=PCI:$id:0:0 --use-display-device=None --force-generate --no-probe-all-gpus -o /etc/X11/xorg.conf.$i
            echo file:$file has been generated.. 
        fi

        ((i++))
    done

    sudo cp /etc/X11/xorg.conf.0 /etc/X11/xorg.conf
    ncores=$i
#    echo i:$i

}


config_X

echo NCOREs:${ncores}


#exit 0

for((i=1;i<ncores;i++));do

#echo $i
#start X with specified configuration
X -config /etc/X11/xorg.conf.$i :$i -sharevts &> /var/log/logX.$i &

done


