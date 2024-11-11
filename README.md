## Description of the analyzer
- This repository contains an analyzer for the ***ttHH Run II hadronic channel***. 
- While the current code focuses on trigger studies, the overall framework is versatile enough for full analysis. 
- We strive to rely solely on ROOT libraries and are in the process of phasing out several “deprecated” libraries ( ex. HypothesisCombinatorics.h, MVAvarsJABDTthh.h, ... ), though this goal hasn’t been fully achieved yet.

## Pre-requisites
A Scientific Linux 7 is required for compiling this code. 

Note that lxplus does not support the el7 environment directly now, so you’ll need to use Singularity with the ```cmssw-el7``` command. If you want to use a different OS version, be sure to update the variables and libraries in **Makefile**.
- Important: Since Singularity doesn’t support job submissions to Condor, use lxplus8 or lxplus9 servers for submitting jobs.
- Libraries: ***ROOT, GSL, and LHAPDF***. cmssw-el7 and CMSSW_10_6_28 are suggested for this purpose.
- TTH libraries: Install tar files


&#9655;	To set up the cmssw environment & install the analyzer:
```bash
# In lxplus..
cmssw-el7
cmsrel CMSSW_10_6_28
cd CMSSW_10_6_28/src && cmsenv
git clone https://github.com/Junghyun-Lee-Physicist/ttHHAnalyzer.git
cp /eos/user/j/junghyun/public/TTH.tar.gz .
tar -zxvf TTH.tar.gz && rm -rf TTH.tar.gz
mv TTH ttHHAnalyzer/.
```

## Compilation to make execution file
```bash
cmssw-el7
cd CMSSW_10_6_28/src/ttHHAnalyzer
cmsenv
source setup.sh  # required for setup
make -j4
```

## Creating a Proxy
The proxy provides the necessary permissions for accessing grid jobs, Condor jobs, and samples on lxplus. If you’re a member of the **CERN CMS VO** with the required permissions, you can generate a proxy using the ```voms-proxy-init``` command.

If you specify an output file for the proxy with the ```--out``` option, set the path in the ```X509_USER_PROXY``` environment variable as shown below to activate proxy permissions:
```bash
voms-proxy-init --voms cms --valid 96:00 --out proxy.cert
export X509_USER_PROXY=proxy.cert
```
Currently, to submit Condor jobs, a **proxy.cert** file with valid time remaining must be present in the analyzer directory. You can check the remaining valid time with the following command:
```bash
voms-proxy-info -file ./proxy.cert --timeleft
```

## Example Local Run
&#9655; To run locally, use the following syntax:
```bash
# ./<exe name> <file-list-path> <output name> <weight> <year> <MC or Data> <sample name>
./ttHHanalyzer_trigger filelistTest/file_ttHH_0.txt test_output_ttHH_0.root 0.00000109763773 2017 MC ttHH_MC_Test
./ttHHanalyzer_trigger filelistTest/file_SingleMuon_C_0.txt test_output_JetHT_C_0.root 1.0 2017 Data JetHT_C_Data_Test
```

## Running with Condor
- For Condor job, you’ll need configuration files in the ```AnalyzerConfig``` directory and a ```proxy.cert``` file within the analyzer directory. 
- You should also have ```sample lists files``` generated by ```DAS query```.
Based on these files, ```submit_job_FH_Trigger.py``` will split and submit jobs. When you create a condor directory, submission files, execution scripts, and logs are automatically generated inside it.


&#9655; To submit condor job:
```bash
python3 submit_job_FH_Trigger.py
```

