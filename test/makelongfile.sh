#! /bin/bash

#Programma per creare un file con
#linee lunghe 100 caratteri

if [[ $# -ne 2 ]]; then
  echo "Type $0 lines filename"
  exit 1
fi

lineStamp="Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam pulvinar arcu pellentesque diam malesuada volutpat. Etiam eu ipsum quis nunc lobortis auctor nec at tortor. Sed volutpat libero quis faucibus tempus. Donec sed orci commodo, auctor magna in, finibus nunc. Duis non finibus dolor. Suspendisse a tempor sem, ac auctor enim. Mauris bibendum tincidunt dictum. Donec in elementum felis. Phasellus ac dolor sem. Phasellus eleifend libero et magna tempus tristique. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Vestibulum euismod efficitur justo. Vestibulum porta turpis id justo faucibus, iaculis efficitur est ultrices. Nunc nec tempus."

let lines=$1
filename=$2
let i=0

echo "Request: $lines lines..."
echo "Dest File: $filename"

if [[ -f "$filename" ]]; then
  rm "$filename"
fi

while [[ i -lt  $lines ]]; do

  echo "$lineStamp" >> "./$filename"

  let i=i+1

done

echo "Done writing file."

