import { useCallback, useEffect, useRef } from "react";

export default function MessageInputBar({
  onMessage,
}: {
  onMessage: (msg: string) => void;
}) {
  const inputRef = useRef<HTMLInputElement>(null);

  const onKeyDown = useCallback(
    (event: KeyboardEvent) => {
      inputRef.current.focus();
      if (event.key === "Enter") {
        const messageContent = inputRef.current?.value;
        if (messageContent) {
          onMessage(messageContent);
          inputRef.current.value = "";
        }
      }
    },
    [onMessage],
  );

  useEffect(() => {
    window.addEventListener("keydown", onKeyDown);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
    };
  }, [onKeyDown]);

  return (
    <div className="flex">
      <div
        className="flex-1 flex p-2"
        style={{ backgroundColor: "var(--boost-light-grey)" }}
      >
        <input
          type="text"
          className="flex-1 text-xl pl-4 pr-4 pt-2 pb-2 border-0 rounded-xl"
          placeholder="Type a message..."
          ref={inputRef}
        />
      </div>
    </div>
  );
}
