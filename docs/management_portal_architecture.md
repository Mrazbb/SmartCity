# Smart City Management Portal: Architecture & Design Document

## 1. Introduction
This document outlines the architecture and technical design of the **Smart City Management Portal**. This web application acts as the unified control plane for the FIWARE-based Digital Twin infrastructure. It simplifies the orchestration of IoT devices, NGSI-LD entities, subscriptions, broker federation, and fine-grained access control, abstracting the complexity of the underlying FIWARE components.

## 2. Technology Stack
*   **Frontend**: ReactJS (Context API, React Query for state management, TailwindCSS for UI).
*   **Backend**: NestJS (Modular Node.js framework).
*   **Database**: PostgreSQL (Internal app state managed via TypeORM).
*   **API Layer**: oRPC (End-to-end typesafe RPC framework connecting React and NestJS).
*   **Target Infrastructure Managed**: Orion-LD (Context Broker), FIWARE IoT Agents (JSON, LoRaWAN), Keycloak (IAM/OIDC), Kong (API Gateway).

---

## 3. Core Modules & Workflows

### 3.1. Device & Entity Management (IoT & Smart Data Models)
**Purpose**: Allow users to seamlessly onboard new sensors (e.g., MQTT, LoRaWAN) and actuators, mapping them directly to official [Smart Data Models](https://smartdatamodels.org/).

*   **Workflow**:
    1.  **Select Input Protocol**: User selects the device type (e.g., `MQTT-JSON` or `LoRaWAN`).
    2.  **Select Smart Data Model**: User selects an entity type (e.g., `AirQualityObserved`, `Streetlight`). The backend fetches the JSON Schema from Smart Data Models to dynamically generate a configuration form in React.
    3.  **Attribute Mapping**: The user maps raw hardware payloads (e.g., `tmp`) to NGSI-LD standard properties (e.g., `temperature`). For MQTT, the user can also specify per-sensor MQTT credentials (username/password).
    4.  **Provisioning**: NestJS makes REST calls to the respective **IoT Agent** (`/iot/devices`) to register the device. The IoT Agent handles the background translation to Orion-LD.

### 3.2. Subscription & Notification Engine
**Purpose**: Enable users to create NGSI-LD subscriptions to forward data to external endpoints or internal analytics engines.

*   **Workflow**:
    1.  **Define Trigger**: User selects entities or types (e.g., Type: `Vehicle`, watchedAttributes: `location`).
    2.  **Define Action**: Specify the target endpoint URL (e.g., an external AI service or QuantumLeap).
    3.  **Apply**: NestJS translates this into an NGSI-LD Subscription payload and posts it to Orion-LD (`/ngsi-ld/v1/subscriptions`).

### 3.3. Federation Manager (Broker-to-Broker Sync)
**Purpose**: Share specific context data with other organizations or central hubs by syncing entities between different Orion-LD deployments.

*   **Workflow**:
    1.  **Target Configuration**: User registers a target external Context Broker (requires URL and auth tokens).
    2.  **Entity Selection**: User selects which local entities to share (e.g., all `AirQualityObserved` in a specific zone).
    3.  **Federation Execution**: The NestJS backend creates a special NGSI-LD Subscription on the *local* Orion-LD, setting the `notification.endpoint.uri` to the *remote* Orion-LD's `/ngsi-ld/v1/entities` (using `notifyFormat: normalized`). Any updates locally are immediately pushed to the remote broker.

### 3.4. Identity, Security & Access Management (OIDC & UMA)
**Purpose**: Treat every NGSI-LD Entity as a protected resource. Allow users to grant third-party apps granular access (RBAC & User-Managed Access - UMA).

*   **Underlying Concept**: **1 Entity = 1 Keycloak Resource**.
*   **Workflow**:
    1.  **Entity Creation Hook**: Whenever a new device/entity is created, NestJS calls the **Keycloak Protection API**. It registers a new Resource (e.g., `urn:ngsi-ld:AirQualityObserved:001`).
    2.  **Scope Definition**: The resource is assigned scopes: `scope:read`, `scope:write`, `scope:delete`.
    3.  **App Onboarding**: A 3rd party developer registers their App in the portal. NestJS creates an OIDC Client in Keycloak for this App.
    4.  **Granting Access**: The portal UI allows the device owner to share their sensor with the 3rd party App. 
    5.  **Policy Generation**: NestJS automatically creates a Keycloak UMA Policy that binds the 3rd party App's Client ID to the `scope:read` of the specific Entity Resource.
    6.  **Enforcement**: When the 3rd party App calls the API Gateway (**Kong**), the Kong OIDC/UMA plugin validates the Access Token. Kong checks with Keycloak if the token has the specific scope for the requested URL (`/ngsi-ld/v1/entities/{id}`).

---

## 4. Internal Database Schema (TypeORM)

While Orion-LD stores the *Context State*, the Management Portal needs an internal relational database (PostgreSQL) to keep track of management metadata:

*   **`DeviceConfig`**: Stores IoT Agent mappings, API keys, hardware IDs, protocol (MQTT/LoRa), and optional per-device MQTT credentials.
*   **`DataModelTemplate`**: Cached JSON schemas from Smart Data Models.
*   **`ExternalApp`**: Records of 3rd party applications registered by users (links to Keycloak Client IDs).
*   **`FederationTarget`**: Credentials and URLs for remote Context Brokers.

## 5. System Orchestration Logic (NestJS Services)

The NestJS backend acts as the orchestrator. Key services include:

*   **`IotAgentService`**: Wraps API calls to `http://iot-agent-json:4041/iot/devices` and `http://iot-agent-lorawan:4041/iot/devices`.
*   **`OrionLdService`**: Wraps API calls to `http://orion-ld:1026/ngsi-ld/v1/` for handling entities and subscriptions.
*   **`KeycloakAdminService`**: Uses the `@keycloak/keycloak-admin-client` to dynamically provision users, clients, resources, and permissions.
*   **`KongAdminService`**: Interacts with Kong's Admin API (`http://kong:8001`) to ensure routes and UMA plugins are correctly bound to exposed entities.

## 6. Development Phasing

1.  **Phase 1 - Scaffold**: Setup Nx/Turborepo workspace. Initialize NestJS, React, TypeORM, and oRPC.
2.  **Phase 2 - Core IoT**: Implement Device Manager (Input -> IoT Agent -> Orion-LD). Connect Smart Data Models API.
3.  **Phase 3 - Subscriptions & Federation**: Build the UI/UX for routing data and cross-broker syncing.
4.  **Phase 4 - Security (Hardest)**: Implement the Keycloak UMA lifecycle (auto-creating resources on entity creation, UI for sharing resources).