#!/bin/bash

docker-compose -f docker-compose-black-server.yml pull
docker-compose -f docker-compose-black-server.yml down
docker-compose -f docker-compose-black-server.yml up -d