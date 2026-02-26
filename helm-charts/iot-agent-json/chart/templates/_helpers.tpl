{{- define "iot-agent-json.fullname" -}}
{{- default .Chart.Name .Values.fullnameOverride | trunc 63 | trimSuffix "-" }}
{{- end }}

{{- define "iot-agent-json.labels" -}}
app.kubernetes.io/name: {{ include "iot-agent-json.fullname" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}

{{- define "iot-agent-json.selectorLabels" -}}
app.kubernetes.io/name: {{ include "iot-agent-json.fullname" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}