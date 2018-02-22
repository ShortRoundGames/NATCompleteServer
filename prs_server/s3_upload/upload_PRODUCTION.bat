@cd ..

@cd www
@cmd /C npm install
@cd ..

@del /AH "www\.ntvs_analysis.dat"

@s3_upload\dos2unix www\bluepill.rb

@s3_upload\tar -cf prs.tar www && s3_upload\bzip2 prs.tar

@s3_upload\s3 put sr-deployment/prs/ prs.tar.bz2 /acl:public-read /key:AKIAJK2NPXYUUIDOVWBA /secret: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 /nogui

@del prs.tar.bz2

@echo All done
@pause
