#!/bin/bash

while getopts u:p:s: option
    do
    case "${option}"
        in
        u) USER=${OPTARG};;
        p) PSWD=${OPTARG};;
        s) SERVER=${OPTARG};;
    esac
done

docker build -t ermiry/black-cerver:production .
echo ${PSWD} | docker login -u ${USER} --password-stdin
docker push ermiry/black-cerver:production

chmod 600 deploy_key && ssh -o StrictHostKeyChecking=no -i deploy_key ermiry@${SERVER} ./start-black-server.sh