// A message in the chat

import NameAvatar from "@/components/NameAvatar";

function formatDate(date: number): string {
  return new Intl.DateTimeFormat("en-US", {
    dateStyle: "medium",
    timeStyle: "short",
  }).format(new Date(date));
}

// A message in the conversation screen, sent by a user that's not ourselves
export const OtherUserMessage = ({
  username,
  content,
  timestamp,
}: {
  username: string;
  content: string;
  timestamp: number;
}) => {
  return (
    <div className="flex flex-row pt-3 pb-3">
      <div className="max-w-11/12 md:max-w-9/12 flex-1 bg-white rounded-lg pt-4 pl-4 md:pl-6 pr-4 md:pr-6">
        <div className="flex flex-row align-middle pb-3">
          <NameAvatar name={username} />
          <div className="flex flex-col justify-center pl-3">
            <p className="text-xs p-0 m-0">{username}</p>
          </div>
        </div>
        <p className="m-0 break-all">{content}</p>
        <p
          className="m-0 pb-1 pr-2 text-xs text-right"
          style={{ color: "var(--boost-grey)" }}
        >
          {formatDate(timestamp)}
        </p>
      </div>
    </div>
  );
};

// A message in the conversation screen, sent by ourselves
export const MyMessage = ({
  content,
  timestamp,
}: {
  content: string;
  timestamp: number;
}) => {
  return (
    <div className="flex flex-row-reverse pt-3 pb-3">
      <div className="max-w-11/12 md:max-w-9/12 flex-1 bg-green-100 rounded-lg pt-4 pl-4 md:pl-6 pr-4 md:pr-6">
        <p className="m-0 break-all">{content}</p>
        <p
          className="m-0 pb-2 pr-2 text-xs text-right"
          style={{ color: "var(--boost-grey)" }}
        >
          {formatDate(timestamp)}
        </p>
      </div>
    </div>
  );
};
