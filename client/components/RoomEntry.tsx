import NameAvatar from "@/components/NameAvatar";
import { useCallback } from "react";

// A clickable chat room card
export default function RoomEntry({
  id,
  name,
  lastMessage,
  selected,
  onClick,
}: {
  id: string;
  name: string;
  lastMessage: string;
  selected: boolean;
  onClick: (roomId: string) => void;
}) {
  const cardStyle = selected
    ? { backgroundColor: "var(--boost-light-grey)" }
    : {};
  const onClickCallback = useCallback(() => onClick(id), [onClick, id]);

  return (
    <div
      className="flex pt-2 pl-3 pr-3 cursor-pointer overflow-hidden"
      onClick={onClickCallback}
    >
      <div
        className="flex-1 flex p-4 rounded-lg overflow-hidden"
        style={cardStyle}
      >
        <div className="pr-3 flex flex-col justify-center">
          <NameAvatar name={name} />
        </div>
        <div className="flex-1 flex flex-col overflow-hidden">
          <p className="m-0 font-bold pb-2" data-testid="room-name">
            {name}
          </p>
          <p className="m-0 overflow-hidden text-ellipsis whitespace-nowrap">
            {lastMessage}
          </p>
        </div>
      </div>
    </div>
  );
}
