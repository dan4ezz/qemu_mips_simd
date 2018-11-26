NAME=vrio_req_spec

all:
	latex $(NAME).tex
	dvips $(NAME).dvi -o $(NAME).ps
	ps2pdf $(NAME).ps

clean:
	rm -f $(NAME).aux \
		$(NAME).dvi \
		$(NAME).log \
		$(NAME).pdf \
		$(NAME).ps
