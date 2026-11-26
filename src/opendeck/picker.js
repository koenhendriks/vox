// The settings page of the Profile and Sound keys: a list to pick from,
// filled by the plugin with Vox's current profiles or sounds. Each page sets
// `window.voxPicker = { setting, list }` before this script runs: the setting
// it stores the choice in, and which of the plugin's lists it shows.
function connectElgatoStreamDeckSocket(port, uuid, registerEvent, info, actionInfo) {
    const action = JSON.parse(actionInfo);
    const { setting, list } = window.voxPicker;
    const select = document.getElementById("choice");
    const empty = document.getElementById("empty");
    let chosen = action.payload.settings[setting] ?? "";
    const socket = new WebSocket("ws://localhost:" + port);

    socket.onopen = () => {
        socket.send(JSON.stringify({ event: registerEvent, uuid }));
        // Asked for, too, in case the host doesn't announce the page itself.
        socket.send(JSON.stringify({ event: "sendToPlugin", action: action.action, context: action.context, payload: { request: "lists" } }));
    };

    socket.onmessage = message => {
        const data = JSON.parse(message.data);
        if (data.event !== "sendToPropertyInspector" || data.payload.event !== "lists")
            return;
        const names = data.payload[list];
        select.innerHTML = "";
        // Keeps a choice that no longer exists visible, rather than quietly
        // pointing the key at something else.
        if (chosen && !names.includes(chosen))
            names.unshift(chosen);
        if (!chosen && names.length)
            add("", "—");
        for (const name of names)
            add(name, name);
        select.value = chosen;
        empty.style.display = names.length ? "none" : "block";
        select.style.display = names.length ? "block" : "none";
    };

    function add(value, text) {
        const option = document.createElement("option");
        option.value = value;
        option.textContent = text;
        select.appendChild(option);
    }

    select.onchange = () => {
        chosen = select.value;
        socket.send(JSON.stringify({ event: "setSettings", context: action.context, payload: { [setting]: chosen } }));
    };
}
