{{- define "mongodb.fullname" -}}
{{- default .Chart.Name .Values.fullnameOverride | trunc 63 | trimSuffix "-" }}
{{- end }}

{{- define "mongodb.labels" -}}
app.kubernetes.io/name: {{ include "mongodb.fullname" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}

{{- define "mongodb.selectorLabels" -}}
app.kubernetes.io/name: {{ include "mongodb.fullname" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}