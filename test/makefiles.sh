#! /bin/bash

#Creo uno script per generare i file da listare

if [[ $# -ne 2 ]]; then
  echo "Type $0 fileNumber dir"
  exit 1
fi

let numeroFile=$1
nomeDirectory=$2
let i=0

echo "Request: $numeroFile files..."
echo "Dest Dir: $nomeDirectory"

if [[ ! -d "$nomeDirectory" ]]; then
  mkdir "$nomeDirectory"
fi


while [[ i -lt $numeroFile  ]]; do

  echo "this is file $i" >> "./$nomeDirectory/file$i"

  let i=i+1
done

echo "Done writing files."
