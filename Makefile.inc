include usb-modules-v2/config.inc

############################################
######## DEFINE SAMPLES/SETS ###############
############################################

SAMPLE_SET_FILE ?= sample_sets.txt
SAMPLE_FILE ?= samples.txt
SAMPLE_SPLIT_FILE ?= samples.split.txt # generate using scripts/prepareFastq.sh [remove underscores from sample names]
SAMPLE_PON_FILE ?= samples.pon.txt
SAMPLE_POOLEDNORM_FILE ?= samples.poolednorm.txt
MERGE_SAMPLE_FILE ?= merge_samples.txt

ifndef MAKEFILE_INC

PROJECT_NAME = $(shell pwd | sed 's:.*/projects/::; s:.*/data/::; s:.*kinglab/::; s:/:_:g')

ifneq ($(wildcard $(SAMPLE_FILE)),)
  SAMPLES ?= $(shell sed '/^\#/d' $(SAMPLE_FILE))
endif
ifneq ($(wildcard $(SAMPLE_PON_FILE)),)
  PANEL_OF_NORMAL_SAMPLES ?= $(shell sed '/^\#/d' $(SAMPLE_PON_FILE))
endif
ifneq ($(wildcard $(SAMPLE_POOLEDNORM_FILE)),)
  POOLED_NORM_SAMPLES ?= $(shell sed '/^\#/d' $(SAMPLE_POOLEDNORM_FILE))
endif
ifneq ($(wildcard $(MERGE_SAMPLE_FILE)),)
  MERGE_SAMPLES ?= $(shell sed '/^\#/d' $(MERGE_SAMPLE_FILE))
endif


get_tumors = $(wordlist 1,$(shell expr $(words $1) - 1),$1)
get_normal = $(lastword $1)
get_space_sets = $(shell sed '/^\#/d; s/\s\+/ /g; s/\s\+$$//;' $(SAMPLE_SET_FILE) | sed -n '$(1)p')
get_underscore_sets = $(shell sed '/^\#/d; s/\s\+/_/g; s/\s\+$$//;' $(SAMPLE_SET_FILE) | sed -n '$(1)p')

ifneq ($(wildcard $(SAMPLE_SET_FILE)),)
  NUM_SETS := $(shell sed '/^\#/d' $(SAMPLE_SET_FILE) | wc -l | cut -f 1 -d' ')
  SETS_SEQ := $(shell seq 1 $(NUM_SETS))
  $(foreach i,$(SETS_SEQ), \
	  $(eval set.$i := $(call get_space_sets,$i)))
  # define set_lookup.$(sample)
  $(foreach i,$(SETS_SEQ), \
	  $(foreach sample,$(set.$i), \
		  $(eval set_lookup.$(sample) := $i)))
  # define SAMPLE_SETS to contain sample_sample_normal
  $(foreach i,$(SETS_SEQ), \
	  $(eval SAMPLE_SETS += $(call get_underscore_sets,$i)))
  # tumor.SET => tumors
  $(foreach i,$(SETS_SEQ), \
	$(eval tumor.$(call get_underscore_sets,$i) := $(call get_tumors,$(set.$i))))
  # normal.SET => normal
  $(foreach i,$(SETS_SEQ), \
	$(eval normal.$(call get_underscore_sets,$i) := $(call get_normal,$(set.$i))))
  NORMAL_SAMPLES = $(filter-out poolednorm,$(foreach i,$(SETS_SEQ),$(call get_normal,$(set.$i))))
  TUMOR_SAMPLES = $(foreach i,$(SETS_SEQ),$(call get_tumors,$(set.$i)))
  SAMPLE_PAIRS = $(foreach set,$(SAMPLE_SETS),$(foreach tumor,$(tumor.$(set)),$(tumor)_$(normal.$(set))))
  # define tumor.pair and normal.pair to retrieve tumor/normal from pairs
  $(foreach set,$(SAMPLE_SETS), \
	  $(foreach tumor,$(tumor.$(set)), \
		  $(eval tumor.$(tumor)_$(normal.$(set)) := $(tumor))))
  $(foreach set,$(SAMPLE_SETS), \
	  $(foreach tumor,$(tumor.$(set)), \
		  $(eval normal.$(tumor)_$(normal.$(set)) := $(normal.$(set)))))
  # tumor_normal.$(tumor) to retrieve tumor_normal
  $(foreach set,$(SAMPLE_SETS), \
	  $(foreach tumor,$(tumor.$(set)), \
		  $(eval tumor_normal.$(tumor) := $(tumor)_$(normal.$(set)))))
  $(foreach set,$(SAMPLE_SETS), \
	  $(foreach tumor,$(tumor.$(set)), \
		  $(eval tumor_normal.$(normal.$(set)) := $(tumor)_$(normal.$(set)))))
  # set.$(sample) to retrieve sample sets (underscores)
  $(foreach i,$(SETS_SEQ), \
	  $(foreach sample,$(set.$i), \
		  $(eval set.$(sample) := $(word $i,$(SAMPLE_SETS)))))
  UNMATCHED_SAMPLES = $(shell sed '/^\#/d' $(SAMPLE_FILE) $(SAMPLE_SET_FILE) | tr ' ' '\n' | sort | uniq -u)

  SAMPLE_SET_PAIRS = $(shell echo "$(SAMPLE_PAIRS) $(SAMPLE_SETS)" | tr ' ' '\n' | sort | uniq)
  $(foreach set,$(SAMPLE_SET_PAIRS), \
	$(eval samples.$(set) := $(shell echo "$(set)" | sed 's/_/ /g')))
endif

ifneq ($(wildcard $(SAMPLE_SPLIT_FILE)),)
  A = $(shell sed '/^\#/d' $(SAMPLE_SPLIT_FILE) | cut -f1)
  B = $(shell sed '/^\#/d' $(SAMPLE_SPLIT_FILE) | cut -f2)
  $(foreach i,$(shell seq 1 $(words $(A))),$(eval split.$(word $i,$(A)) += $(word $i,$(B))))
  UNSPLIT_SAMPLES = $(B)
  SPLIT_SAMPLES = $(shell sed '/^\#/d' $(SAMPLE_SPLIT_FILE) | cut -f1 | sort | uniq)
endif

ALL_SAMPLES = $(SAMPLE_PAIRS) $(SAMPLES)
COUNT_SAMPLES = $(shell expr `sed 's/\..*//; s:.*/::' <<< $1 | grep -o "_" | wc -l` + 1)

############################################
########### DEFINE RESOURCES ###############
############################################

RESOURCE_REQ ?= medium
include usb-modules-v2/resources.inc

#####################################
### DEFINE BASIC FUNCTIONS ##########
#####################################

NO_RM ?= false
ifeq ($(NO_RM),true)
    RM := touch
    RMR = touch
else
    RM := rm -f
    RMR := rm -r
endif

# define $(,) and $( ) for escaping commas and spaces
, := ,
space :=
space +=
$(space) :=
$(space) +=

# $(call strip-suffix,filename)
strip-suffix = $(firstword $(subst ., ,$(1)))

LINK = ln -svf $(notdir $1) $(2) && touch $1
NOW := $(shell date +"%F")
MKDIR = mkdir -p -m 775
MKDIRS = $(MKDIR) $(LOGDIR)/$(@D) $(@D)
LOG = $(LOGDIR)/$(@).log
UMASK = 002

INIT = module purge; umask $(UMASK); $(MKDIRS); set -o pipefail;
PURGE_AND_LOAD = module purge && module load $1

MD5 = md5sum $(@:.md5=) > $@
CHECK_MD5 = for i in {0..20}; do if md5sum -c $(filter %.md5,$^); then break; fi; sleep 5; done;
<M = $(<:.md5=)
^M = $(^:.md5=)
@M = $(@:.md5=)
<<M = $(word 2,$(^M))
<<<M = $(word 3,$(^M))

# $(call CHECK_VCF,vcf.file,target,command)
# check for valid header, ie non-empty vcf file
CHECK_VCF = if [ `grep -v '^\#' $1 | wc -l` -eq 0 ] && [ `grep '^\#CHROM' $1 | wc -l` -eq 1 ]; then cp $1 $2; else $3; fi
CHECK_MAF = if [ `grep -v '^\#' $1 | wc -l` -eq 1 ] && [ `grep '^Hugo_Symbol' $1 | wc -l` -eq 1 ]; then cp $1 $2; else $3; fi
CHECK_VCF_CMD = if [ `grep -v '^\#' $1 | wc -l` -eq 0 ] && [ `grep '^\#CHROM' $1 | wc -l` -eq 1 ]; then $2; else $3; fi

<< = $(word 2,$^)
<<< = $(word 3,$^)
<<<< = $(word 4,$^)
4< = $(word 4,$^)
<<<<< = $(word 5,$^)
5< = $(word 5,$^)

define R_INIT
dir.create('$(@D)', showWarnings = F, recursive = T)
qw <- function(x) unlist(strsplit(x, "[[:space:]]+"))
endef

##########################################
##### DEFINE THE MAIN RUN FUNCTION #######
##########################################

RUN_OPTS = $(if $(findstring false,$(USE_CLUSTER)),-l) -o $@ $(if $(CLUSTER_ENGINE),-g $(CLUSTER_ENGINE)) \
  -S $(SHELL) --log_file $(LOG) $(if $(PROJECT_NAME),-p $(PROJECT_NAME)) -c

## RUN is the main function!!
## arg1 = number of cores
## arg2 = memory per core
## arg3 = wall time

RUN = $(call PURGE_AND_LOAD,$(PYTHON_MODULE)); echo "\
$(call PURGE_AND_LOAD,$4); " $5 | python usb-modules-v2/scripts/run.py $(RUN_OPTS) -n $1 -m $2 -w $3

RUN += $(if $(findstring SLURM,$(CLUSTER_ENGINE)),\
$(if $(findstring $(QOS_2w),$3),--qos 2weeks,\
$(if $(findstring $(QOS_1w),$3),--qos 1week,\
$(if $(findstring $(QOS_1d),$3),--qos 1day,\
$(if $(findstring $(QOS_6h),$3),--qos 6hours,\
$(if $(findstring $(QOS_30min),$3),--qos 30min,infinite))))),)

###############################################################
####### DEFINE PATHS/NAMES FOR SCRIPTS, BINARIES, MODULES #####
###############################################################

USR_DIR ?= /scicore/home/terracci/GROUP/usr_nobackup
JAR_DIR ?= $(USR_DIR)/local/lib/java
SCRIPTS_DIR ?= $(USR_DIR)/local/scripts
BIN_DIR ?= $(USR_DIR)/local/bin
MODULE_SCRIPTS_DIR ?= usb-modules-v2/scripts

export TMPDIR := /scicore/scratch
export PATH := $(USR_DIR):$(PATH)

SHELL ?= /bin/bash
UNZIP ?= /usr/bin/unzip

ifeq (SGE,$(CLUSTER_ENGINE))
PERL_MODULE ?= Perl/5.22.2-goolf-1.7.20
R_MODULE ?= R/3.3.0-goolf-1.7.20
JAVA6_MODULE ?= Java/1.6.0_24
JAVA7_MODULE ?= Java/1.7.0_80
JAVA8_MODULE ?= Java/1.8.0_92
PYTHON_MODULE ?= Python/3.5.1-goolf-1.4.10
SAMTOOLS_MODULE ?= SAMtools/1.2-goolf-1.4.10
BCFTOOLS_MODULE ?= BCFtools/1.2-goolf-1.4.10
BEDTOOLS_MODULE ?= BEDTools/2.25.0-goolf-1.4.10
IGVTOOLS_MODULE ?= IGVTools/2.3.60-Java-1.7.0_21
VCFTOOLS_MODULE ?= VCFtools/0.1.14-goolf-1.4.10-Perl-5.16.3
BCL2FASTQ_MODULE ?= bcl2fastq/2.17.1.14-goolf-1.7.20
TRIM_GALORE_MODULE = Trim_Galore/0.4.1-goolf-1.4.10-Perl-5.16.3
TABIX_MODULE ?= tabix/0.2.6-goolf-1.4.10
FASTQC_MODULE ?= FastQC/0.11.4-Java-1.7.0_21
BOWTIE2_MODULE ?= Bowtie2/2.2.6-goolf-1.4.10
BWA_MODULE ?= BWA/0.7.12-goolf-1.4.10
TOPHAT_MODULE ?= TopHat/2.1.1-goolf-1.7.20
STAR_MODULE ?= STAR/2.5.2a-goolf-1.7.20
STAR_FUSION_MODULE ?= STAR-Fusion/0.6.0-goolf-1.4.10-Perl-5.16.3
HISAT2_MODULE ?= HISAT2/2.0.4-goolf-1.7.20
STRINGTIE_MODULE ?= StringTie/1.3.3-goolf-1.7.20
SNP_EFF_MODULE ?= snpEff/4.1l-Java-1.7.0_75
CUFFLINKS_MODULE ?= Cufflinks/2.2.1-Linux_x86_64
STRINGTIE_MODULE ?= StringTie/1.3.3-goolf-1.7.20
RSEM_MODULE ?= RSEM/1.2.31-goolf-1.7.20
endif

ifeq (SLURM,$(CLUSTER_ENGINE))
PERL_MODULE ?= Perl/5.22.2-goolf-1.7.20
R_MODULE ?= R/3.4.2-goolf-1.7.20
JAVA6_MODULE ?= Java/1.6.0_24
JAVA7_MODULE ?= Java/1.7.0_80
JAVA8_MODULE ?= Java/1.8.0_92
PYTHON_MODULE ?= Python/3.5.2-goolf-1.7.20
SAMTOOLS_MODULE ?= SAMtools/1.3-goolf-1.7.20
BCFTOOLS_MODULE ?= BCFtools/1.5-goolf-1.7.20
BEDTOOLS_MODULE ?= BEDTools/2.26.0-goolf-1.7.20
IGVTOOLS_MODULE ?= IGVTools/2.3.60-Java-1.7.0_21
VCFTOOLS_MODULE ?= VCFtools/0.1.14-goolf-1.7.20-Perl-5.22.2
BCL2FASTQ_MODULE ?= bcl2fastq/2.17.1.14-goolf-1.7.20
TRIM_GALORE_MODULE = Trim_Galore/0.4.4-goolf-1.7.20-Perl-5.22.2
HTSLIB_MODULE ?= HTSlib/1.3.1-goolf-1.7.20
TABIX_MODULE ?= $(HTSLIB_MODULE)
FASTQC_MODULE ?= FastQC/0.11.4-Java-1.7.0_21
BOWTIE2_MODULE ?= Bowtie2/2.3.2-goolf-1.7.20
BWA_MODULE ?= BWA/0.7.13-goolf-1.7.20
TOPHAT_MODULE ?= TopHat/2.0.6.Linux_x86_64
STAR_MODULE ?= STAR/2.5.2a-goolf-1.7.20
STAR_FUSION_MODULE ?= STAR-Fusion/0.6.0-goolf-1.7.20-Perl-5.22.2
HISAT2_MODULE ?= HISAT2/2.0.4-goolf-1.7.20
STRINGTIE_MODULE ?= StringTie/1.3.3-goolf-1.7.20
SNP_EFF_MODULE ?= snpEff/4.1l-Java-1.7.0_75
CUFFLINKS_MODULE ?= Cufflinks/2.2.1-Linux_x86_64
STRINGTIE_MODULE ?= StringTie/1.3.3-goolf-1.7.20
RSEM_MODULE ?= RSEM/1.2.31-goolf-1.7.20
OPENBLAS_MODULE ?= OpenBLAS/0.2.13-GCC-4.8.4-LAPACK-3.5.0
endif



PERL ?= perl
JAVA ?= java -Djava.io.tmpdir=$(TMPDIR)
PYTHON ?= python
PYTHON_ENV_ACTIVATE ?= source /scicore/home/terracci/GROUP/virtual_env/python/bin/activate

R_LIBS ?= $(USR_DIR)/$(R_MODULE)/lib64/R/library
R ?= export R_LIBS=$(R_LIBS); R
RSCRIPT ?= export R_LIBS=$(R_LIBS); Rscript
KNIT ?= $(RSCRIPT) $(MODULE_SCRIPTS_DIR)/knit.R
SWEAVE ?= $(RSCRIPT) $(MODULE_SCRIPTS_DIR)/Sweave.R
RBIND ?= $(RSCRIPT) $(MODULE_SCRIPTS_DIR)/rbind.R

SAMTOOLS ?= samtools
BCFTOOLS ?= bcftools
BEDTOOLS ?= bedtools
IGVTOOLS ?= igvtools
VCFTOOLS ?= vcftools
VCFTOOLS_MERGE ?= vcf-merge
BCL2FASTQ ?= bcl2fastq
TRIM_GALORE ?= trim_galore
BGZIP ?= bgzip
TABIX ?= tabix -p vcf
BAM_READCOUNT ?= $(BIN_DIR)/bam-readcount

VCF_SORT ?= $(PERL) $(SCRIPTS_DIR)/vcfsorter.pl
FASTQ_TRIMMER = $(PERL) usb-modules-v2/fastq_tools/trimFastq.pl

FASTQ_CHUNKS := 10
FASTQ_CHUNK_SEQ := $(shell seq 1 $(FASTQ_CHUNKS))

FASTQC ?= fastqc
FASTQC_SUMMARY_PLOT = $(RSCRIPT) usb-modules-v2/qc/fastqcSummaryPlot.R
PLOT_RNASEQ_METRICS = $(RSCRIPT) usb-modules-v2/qc/plotRnaseqMetrics.R

CLUSTER_VCF = $(RSCRIPT) usb-modules-v2/qc/clusterVcf.R

BOWTIE2 ?= bowtie2
BWA_ALN ?= bwa aln
BWA_SAMPE ?= bwa sampe
BWA_MEM ?= bwa mem
TOPHAT ?= tophat
TOPHAT2 ?= tophat2
STAR ?= STAR
STAR_FUSION ?= STAR-Fusion
HISAT2 ?= hisat2
STRINGTIE ?= stringtie

PICARD = $(JAVA) -Xmx$(2) -jar $(JAR_DIR)/picard-tools-2.4.1/picard.jar $(1)
PICARD += VALIDATION_STRINGENCY=LENIENT MAX_RECORDS_IN_RAM=4000000 TMP_DIR=$(TMPDIR)
PICARD += REFERENCE_SEQUENCE=$(REF_FASTA) CREATE_INDEX=true

GATK = $(JAVA) -Xmx$(2) -jar $(JAR_DIR)/GenomeAnalysisTK-3.6/GenomeAnalysisTK-3.6.jar 
GATK += -S LENIENT -T $(1) -filterNoBases
GATK37 = $(JAVA) -Xmx$(2) -jar $(JAR_DIR)/GenomeAnalysisTK-3.7/GenomeAnalysisTK-3.7.jar 
GATK37 += -S LENIENT -T $(1) -filterNoBases

FIX_GATK_VCF = $(PERL) usb-modules-v2/variant_callers/fix_vcf_scripts/fix_gatk_vcf.pl
FIX_VARSCAN_VCF = $(PERL) usb-modules-v2/variant_callers/fix_vcf_scripts/fix_varscan_vcf.pl
FIX_STRELKA_VCF = $(PERL) usb-modules-v2/variant_callers/fix_vcf_scripts/fix_strelka_vcf.pl
FIX_TVC_VCF = $(PERL) usb-modules-v2/variant_callers/fix_vcf_scripts/fix_tvc_vcf.pl

MUTECT ?= $(JAVA) -Xmx$(1) -jar $(JAR_DIR)/muTect-1.1.4.jar --analysis_type MuTect


CONFIGURE_STRELKA = $(PERL) $(USR_DIR)/local/strelka_workflow-1.0.15/bin/configureStrelkaWorkflow.pl
STRELKA_CONFIG = usb-modules-v2/variant_callers/somatic/strelka_config_bwa_default.ini

TVC_ROOT_DIR ?= $(USR_DIR)/local/tvc-5.8.0-CentOS_7.3-x86_64-binary
TVC = $(PYTHON_ENV_ACTIVATE) && $(TVC_ROOT_DIR)/bin/variant_caller_pipeline.py
TVC_SOMATIC_JSON = usb-modules-v2/variant_callers/somatic/TVC_somatic_parameters.json
TVC_SENSITIVE_JSON = usb-modules-v2/variant_callers/somatic/TVC_somatic_parameters_sensitive.json
TVC_MOTIF ?= $(TVC_ROOT_DIR)/share/TVC/sse/ampliseqexome_germline_p1_hiq_motifset.txt

VARSCAN_JAR ?= $(JAR_DIR)/varscan/VarScan.v2.4.3.jar
VARSCAN = $(JAVA) -Xmx6G -jar $(VARSCAN_JAR)

SNP_EFF_CONFIG ?= usb-modules-v2/snpEff.config
SNP_EFF = $(JAVA) -Xmx$(RESOURCE_REQ_LOW_MEM_JAVA) -jar \$$EBROOTSNPEFF/snpEff.jar
SNP_EFF_OPTS = -i vcf -o vcf -c $(SNP_EFF_CONFIG) $(SNP_EFF_FLAGS)
SNP_SIFT = $(JAVA) -Xmx$(RESOURCE_REQ_LOW_MEM_JAVA) -jar \$$EBROOTSNPEFF/SnpSift.jar
SNP_SIFT_OPTS = -c $(SNP_EFF_CONFIG)
VCF_EFF_ONE_PER_LINE = $(PERL) \$$EBROOTSNPEFF/scripts/vcfEffOnePerLine.pl
VCF_JOIN_EFF = $(PERL) $(MODULE_SCRIPTS_DIR)/joinEff.pl

ADD_GENE_LIST_ANNOTATION = $(RSCRIPT) usb-modules-v2/vcf_tools/addGeneListAnnotationToVcf.R
ANNOTATE_FACETS_VCF = $(RSCRIPT) usb-modules-v2/copy_number/annotateFacets2Vcf.R

FACETS_SNP_PILEUP = $(BIN_DIR)/snp-pileup
FACETS = $(RSCRIPT) usb-modules-v2/copy_number/facetsRun.R
FACETS_GENE_CN = $(RSCRIPT) usb-modules-v2/copy_number/facetsGeneCN.R
FACETS_PLOT_GENE_CN = $(RSCRIPT) usb-modules-v2/copy_number/facetsGeneCNPlot.R
FACETS_CCF = $(RSCRIPT) usb-modules-v2/copy_number/facetsCCF.R

FACETS_GENE_CN_OPTS = $(if $(TARGETS_FILE_GENES),--genesFile $(TARGETS_FILE_GENES))


MUTATION_SUMMARY_RSCRIPT = $(RSCRIPT) usb-modules-v2/summary/mutation_summary_excel.R

CBS_SEGMENTCNV = $(RSCRIPT) usb-modules-v2/copy_number/varscanCNVSegment.R
VARSCAN_GENE_CN = $(RSCRIPT) usb-modules-v2/copy_number/varscanCNVGeneCN.R

MUTSIGCV ?= $(USR_DIR)/local/MutSigCV_1.4/run_MutSigCV.sh
MCR ?= $(USR_DIR)/local/MCR_R2013a_glnxa64/v81/

GISTIC ?= $(USR_DIR)/local/gistic_2.0.23/gp_gistic2_from_seg

CUFFLINKS ?= cufflinks
CUFFCMP ?= currcompare

RSEM_CALC_EXPR ?= rsem-calculate-expression
RSEM_GEN_DATA_MATRIX ?= usb-modules-v2/rnaseq/rsem-generate-data-matrix.pl

##VCF2MAF ?= $(PERL) $(SCRIPTS_DIR)/vcf2maf.pl
#MAF2MAF ?= $(PERL) $(SCRIPTS_DIR)/maf2maf.pl
#MAF2VCF ?= $(PERL) $(SCRIPTS_DIR)/maf2vcf.pl
#VEP_PATH ?= /opt/common/CentOS_6/vep/v82
#VEP_DATA ?= /opt/common/CentOS_6/vep/v82

#BAM2FASTX = $(HOME)/usr/bin/bam2fastx
#GET_INSERT_SIZE ?= $(HOME)/share/usr/bin/getInsertSize.py

#TMAP = $(HOME)/share/usr/bin/tmap

#MUT_FREQ_REPORT = modules/variant_callers/somatic/mutectReport.Rmd

#VARIANT_EVAL_GATK_REPORT = $(RSCRIPT) usb-modules-v2/variant_callers/variantEvalGatkReport.R

endif
MAKEFILE_INC = true