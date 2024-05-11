#!/bin/bash

# Set a counter to batch files
counter=0
batch_size=20  # Number of files per batch
total=1
# Loop over a pattern, adjust the pattern as necessary
for file in cpp/tensorrt_llm/kernels/contextFusedMultiHeadAttention/cubin/fmha_v2*.cpp; do
    git add "$file"
    let counter+=1

    # When batch size is reached, commit and push
    if [ "$counter" -eq "$batch_size" ]; then
        git commit -m "Adding batch of fmha files($total)"
        git push origin main  # Adjust the branch name as necessary
        counter=0  # Reset counter after each batch
        let total+=1
    fi
done

# Final commit for any remaining files
if [ "$counter" -gt 0 ]; then
    git commit -m "Adding final batch of fmha files"
    git push origin main
fi
