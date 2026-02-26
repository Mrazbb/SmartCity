{{- define "mosquitto.fullname" -}}
{{- default .Chart.Name .Values.fullnameOverride | trunc 63 | trimSuffix "-" }}
{{- end }}

{{- define "mosquitto.labels" -}}
app.kubernetes.io/name: {{ include "mosquitto.fullname" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}

{{- define "mosquitto.selectorLabels" -}}
app.kubernetes.io/name: {{ include "mosquitto.fullname" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}