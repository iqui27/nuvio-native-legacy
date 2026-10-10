FROM python:3-alpine
RUN adduser -D -u 1000 app
WORKDIR /app
COPY servidor.py /app/servidor.py
ENV PYTHONUNBUFFERED=1
# O implantar.sh sobrescreve com --user do dono da pasta de respostas.
USER app
EXPOSE 8000
CMD ["python", "/app/servidor.py"]
